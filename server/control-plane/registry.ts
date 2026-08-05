export interface PeerRecord {
  peer_id: string;
  tunnel_host: string;
  expert_start: number;
  expert_end: number;
  busy: boolean;
  last_seen: number;
  credits: number;
  session_started_at: number | null;
  token_hash: string;
}

export interface QueueJob {
  id: string;
  peer_id: string;
  prompt: string;
  expert_start: number;
  expert_end: number;
  status: "queued" | "ready" | "running" | "done" | "expired";
  created_at: number;
  assigned_peer_id?: string;
}

interface AuthBody {
  peer_id?: string;
  peer_token?: string;
  job_id?: string;
  expert_start?: number;
  expert_end?: number;
  prompt?: string;
  busy?: boolean;
  tunnel_host?: string;
}

interface Env {
  ACCESS_TOKEN_TTL_SEC?: string;
  SESSION_TTL_SEC?: string;
  PEER_STALE_SEC?: string;
  CF_ACCESS_CLIENT_ID?: string;
  CF_ACCESS_CLIENT_SECRET?: string;
  MIN_VOLUNTEERS?: string;
  MAX_CONCURRENT_PROMPTS?: string;
  QUEUE_JOB_TTL_HOURS?: string;
  FEATURE_CREDITS?: string;
  CREDITS_PER_SEC?: string;
}

export class Registry implements DurableObject {
  private peers: Map<string, PeerRecord> = new Map();
  private queue: QueueJob[] = [];
  private rateBuckets = new Map<string, number[]>();

  constructor(private state: DurableObjectState, private env: Env) {}

  async fetch(request: Request): Promise<Response> {
    const url = new URL(request.url);
    const path = url.pathname.replace(/^\/do/, "");
    const clientIp = request.headers.get("CF-Connecting-IP") || "unknown";
    await this.loadAll();
    this.evictStalePeers();
    this.expireStaleJobs();
    this.tryPromoteQueue();

    if ((path === "/stats" || path === "/status") && request.method === "GET") {
      return json(this.statusPayload());
    }
    if (path === "/register" && request.method === "POST") {
      if (!this.rateLimit(`reg:${clientIp}`, 6, 60000)) return json({ error: "rate limit" }, 429);
      return this.handleRegister(await request.json());
    }
    if (path === "/deregister" && request.method === "POST") {
      const body = (await request.json()) as AuthBody;
      if (!this.authPeer(body.peer_id, body.peer_token)) return json({ error: "unauthorized" }, 403);
      this.peers.delete(body.peer_id!);
      await this.persist();
      return json({ ok: true });
    }
    if (path === "/session/release" && request.method === "POST") {
      return this.handleRelease(await request.json());
    }
    if (path === "/session" && request.method === "POST") {
      const body = (await request.json()) as AuthBody;
      if (!this.rateLimit(`sess:${body.peer_id}`, 6, 60000)) return json({ error: "rate limit" }, 429);
      return this.handleSession(body);
    }
    if (path === "/queue" && request.method === "POST") {
      const body = (await request.json()) as AuthBody;
      if (!this.rateLimit(`q:${body.peer_id}`, 3, 60000)) return json({ error: "rate limit" }, 429);
      return this.handleQueueSubmit(body);
    }
    if (path.startsWith("/queue/") && request.method === "GET") {
      return this.handleQueuePoll(path.slice("/queue/".length));
    }
    return json({ error: "not found" }, 404);
  }

  private cfgNum(key: keyof Env, fallback: number): number {
    const v = this.env[key];
    return v ? parseInt(v, 10) : fallback;
  }

  private minVolunteers(): number {
    return this.cfgNum("MIN_VOLUNTEERS", 3);
  }

  private maxConcurrent(): number {
    return this.cfgNum("MAX_CONCURRENT_PROMPTS", 3);
  }

  private swarmReady(): boolean {
    return this.peers.size >= this.minVolunteers();
  }

  private activePrompts(): number {
    return this.queue.filter((j) => j.status === "running" || j.status === "ready").length;
  }

  private async hashToken(token: string): Promise<string> {
    const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(token));
    return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
  }

  private async authPeer(peerId?: string, token?: string): Promise<boolean> {
    if (!peerId || !token) return false;
    const p = this.peers.get(peerId);
    if (!p?.token_hash) return false;
    return (await this.hashToken(token)) === p.token_hash;
  }

  private rateLimit(key: string, limit: number, windowMs: number): boolean {
    const now = Date.now();
    let hits = (this.rateBuckets.get(key) || []).filter((t) => now - t < windowMs);
    if (hits.length >= limit) return false;
    hits.push(now);
    this.rateBuckets.set(key, hits);
    return true;
  }

  private statusPayload() {
    const min = this.minVolunteers();
    const pool = this.peers.size;
    const ready = this.swarmReady();
    return {
      ok: true,
      volunteer_pool_size: pool,
      idle_volunteers: [...this.peers.values()].filter((p) => !p.busy).length,
      min_volunteers: min,
      waiting_for: ready ? 0 : Math.max(0, min - pool),
      swarm_ready: ready,
      max_concurrent_prompts: this.maxConcurrent(),
      active_prompts: this.activePrompts(),
      queue_depth: this.queue.filter((j) => j.status === "queued").length,
    };
  }

  private fifoQueued(): QueueJob[] {
    return this.queue
      .filter((j) => j.status === "queued")
      .sort((a, b) => a.created_at - b.created_at);
  }

  private queuePosition(job: QueueJob): number {
    if (job.status !== "queued") return 0;
    const idx = this.fifoQueued().findIndex((j) => j.id === job.id);
    return idx >= 0 ? idx + 1 : 0;
  }

  private coversExpertRange(p: PeerRecord, start: number, end: number): boolean {
    return !p.busy && p.expert_start <= start && p.expert_end >= end;
  }

  private releaseVolunteer(peerId: string) {
    const p = this.peers.get(peerId);
    if (!p) return;
    p.busy = false;
    p.session_started_at = null;
    p.last_seen = Date.now();
    this.peers.set(peerId, p);
  }

  private tryPromoteQueue() {
    if (!this.swarmReady()) return;
    let slots = this.maxConcurrent() - this.activePrompts();
    for (const job of this.fifoQueued()) {
      if (slots <= 0) break;
      job.status = "ready";
      slots--;
    }
  }

  private expireStaleJobs() {
    const ttlMs = this.cfgNum("QUEUE_JOB_TTL_HOURS", 24) * 3600000;
    const now = Date.now();
    let changed = false;
    for (const job of this.queue) {
      if (job.status !== "queued" && job.status !== "ready" && job.status !== "running") continue;
      const orphan =
        job.assigned_peer_id && (job.status === "running" || job.status === "ready") &&
        !this.peers.has(job.assigned_peer_id);
      if (now - job.created_at > ttlMs || orphan) {
        if (job.assigned_peer_id) this.releaseVolunteer(job.assigned_peer_id);
        job.status = "expired";
        job.assigned_peer_id = undefined;
        changed = true;
      }
    }
    if (changed) void this.persist();
  }

  private evictStalePeers() {
    const maxAge = parseInt(this.env.PEER_STALE_SEC || "120", 10) * 1000;
    const now = Date.now();
    let changed = false;
    for (const [id, p] of this.peers) {
      if (!p.busy && now - p.last_seen > maxAge) {
        this.peers.delete(id);
        changed = true;
      }
    }
    if (changed) void this.persist();
  }

  private async handleRegister(body: AuthBody) {
    if (!body.peer_id || !body.peer_token) return json({ error: "peer_id and peer_token required" }, 400);
    const hash = await this.hashToken(body.peer_token);
    const prev = this.peers.get(body.peer_id);
    if (prev?.token_hash && prev.token_hash !== hash) return json({ error: "unauthorized" }, 403);
    const now = Date.now();
    const rec: PeerRecord = {
      peer_id: body.peer_id,
      tunnel_host: body.tunnel_host || body.peer_id,
      expert_start: body.expert_start ?? 0,
      expert_end: body.expert_end ?? 383,
      busy: !!body.busy,
      last_seen: now,
      credits: prev?.credits ?? 0,
      session_started_at: prev?.session_started_at ?? null,
      token_hash: hash,
    };
    this.peers.set(rec.peer_id, rec);
    this.tryPromoteQueue();
    await this.persist();
    return json({ ok: true, peer_id: rec.peer_id, ...this.statusPayload() });
  }

  private async handleRelease(body: AuthBody) {
    const job = body.job_id ? this.queue.find((j) => j.id === body.job_id) : undefined;
    const requesterOk = job && (await this.authPeer(job.peer_id, body.peer_token));
    const volunteerOk = job?.assigned_peer_id && (await this.authPeer(job.assigned_peer_id, body.peer_token));
    const directOk = !job && (await this.authPeer(body.peer_id, body.peer_token));
    if (!requesterOk && !volunteerOk && !directOk) return json({ error: "unauthorized" }, 403);

    const volId = job?.assigned_peer_id || body.peer_id;
    const p = volId ? this.peers.get(volId) : undefined;
    if (p) {
      if (this.env.FEATURE_CREDITS === "1" && p.session_started_at) {
        const sec = Math.floor((Date.now() - p.session_started_at) / 1000);
        p.credits += sec * (parseInt(this.env.CREDITS_PER_SEC || "1", 10) || 1);
      }
      p.busy = false;
      p.session_started_at = null;
      p.last_seen = Date.now();
      this.peers.set(p.peer_id, p);
    }
    if (job && job.status === "running") job.status = "done";
    this.tryPromoteQueue();
    await this.persist();
    return json({ ok: true });
  }

  private accessConfigured(): boolean {
    return !!(this.env.CF_ACCESS_CLIENT_ID && this.env.CF_ACCESS_CLIENT_SECRET);
  }

  private async handleSession(req: AuthBody) {
    if (!this.swarmReady()) {
      return json({ error: "swarm not ready", ...this.statusPayload() }, 503);
    }
    if (!req.job_id || !req.peer_id) {
      return json({ error: "prompts require POST /queue then /session with job_id" }, 403);
    }
    if (!(await this.authPeer(req.peer_id, req.peer_token))) {
      return json({ error: "unauthorized" }, 403);
    }
    if (!this.accessConfigured()) {
      return json({ error: "production requires Cloudflare Access", access_mode: "disabled" }, 503);
    }
    const job = this.queue.find((j) => j.id === req.job_id);
    if (!job || job.status !== "ready" || job.peer_id !== req.peer_id) {
      return json({ error: "job not ready", status: job?.status ?? "missing" }, 409);
    }
    const start = req.expert_start ?? job.expert_start;
    const end = req.expert_end ?? job.expert_end;
    const vol = [...this.peers.values()].find((p) => this.coversExpertRange(p, start, end));
    if (!vol) return json({ error: "no idle volunteer", volunteer_pool_size: this.peers.size }, 409);
    job.status = "running";
    job.assigned_peer_id = vol.peer_id;
    vol.busy = true;
    vol.session_started_at = Date.now();
    vol.last_seen = Date.now();
    this.peers.set(vol.peer_id, vol);
    await this.persist();
    const ttl = this.sessionTtl();
    return json({
      tunnel_host: vol.tunnel_host,
      access_client_id: this.env.CF_ACCESS_CLIENT_ID,
      access_client_secret: this.env.CF_ACCESS_CLIENT_SECRET,
      access_token: "",
      local_port: 50053,
      peer_id: vol.peer_id,
      job_id: job.id,
      prompt: job.prompt,
      ttl_sec: ttl,
      access_mode: "cloudflare_access_service_token",
    });
  }

  private sessionTtl(): number {
    const sessionTtl = parseInt(this.env.SESSION_TTL_SEC || "3600", 10);
    return Math.min(parseInt(this.env.ACCESS_TOKEN_TTL_SEC || "3600", 10), sessionTtl + 300);
  }

  private async handleQueueSubmit(req: AuthBody) {
    if (!req.peer_id || !req.prompt) return json({ error: "peer_id and prompt required" }, 400);
    if (!(await this.authPeer(req.peer_id, req.peer_token))) {
      return json({ error: "unknown peer_id — run moontail init first" }, 404);
    }
    const p = this.peers.get(req.peer_id)!;
    const job: QueueJob = {
      id: crypto.randomUUID(),
      peer_id: req.peer_id,
      prompt: req.prompt,
      expert_start: req.expert_start ?? p.expert_start,
      expert_end: req.expert_end ?? p.expert_end,
      status: "queued",
      created_at: Date.now(),
    };
    this.queue.push(job);
    this.tryPromoteQueue();
    await this.persist();
    return json({
      job_id: job.id,
      position: this.queuePosition(job),
      status: job.status,
      swarm_ready: this.swarmReady(),
      poll: `/queue/${job.id}`,
    });
  }

  private async handleQueuePoll(id: string) {
    const job = this.queue.find((j) => j.id === id);
    if (!job) return json({ error: "not found" }, 404);
    return json({
      status: job.status,
      position: this.queuePosition(job),
      swarm_ready: this.swarmReady(),
      waiting_for: this.swarmReady() ? 0 : Math.max(0, this.minVolunteers() - this.peers.size),
    });
  }

  private async loadAll() {
    const stored = await this.state.storage.get<[string, PeerRecord][]>("peers");
    if (stored) {
      this.peers = new Map(stored);
      for (const [, p] of this.peers) {
        if (!p.token_hash) p.token_hash = "";
      }
    }
    const q = await this.state.storage.get<QueueJob[]>("queue");
    if (q) this.queue = q;
  }

  private async persist() {
    await this.state.storage.put("peers", [...this.peers.entries()]);
    await this.state.storage.put("queue", this.queue);
  }
}

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}
