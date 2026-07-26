export interface PeerRecord {
  peer_id: string;
  tunnel_host: string;
  expert_start: number;
  expert_end: number;
  busy: boolean;
  last_seen: number;
}

interface SessionRequest {
  expert_start?: number;
  expert_end?: number;
}

export class Registry implements DurableObject {
  private peers: Map<string, PeerRecord> = new Map();

  constructor(private state: DurableObjectState, private env: { ACCESS_TOKEN_TTL_SEC?: string }) {}

  async fetch(request: Request): Promise<Response> {
    const url = new URL(request.url);
    const path = url.pathname.replace(/^\/do/, "");

    if (path === "/register" && request.method === "POST") {
      const body = (await request.json()) as PeerRecord;
      const rec: PeerRecord = {
        peer_id: body.peer_id,
        tunnel_host: body.tunnel_host || body.peer_id,
        expert_start: body.expert_start ?? 0,
        expert_end: body.expert_end ?? 255,
        busy: !!body.busy,
        last_seen: Date.now(),
      };
      this.peers.set(rec.peer_id, rec);
      await this.state.storage.put("peers", [...this.peers.entries()]);
      return json({ ok: true, peer_id: rec.peer_id });
    }

    if (path === "/session" && request.method === "POST") {
      const req = (await request.json()) as SessionRequest;
      const start = req.expert_start ?? 0;
      const end = req.expert_end ?? 255;
      await this.loadPeers();

      const idle = [...this.peers.values()].filter(
        (p) => !p.busy && p.expert_start <= start && p.expert_end >= end,
      );
      if (idle.length === 0) {
        return json({ error: "no idle volunteer", volunteer_pool_size: this.peers.size }, 409);
      }

      const pick = idle[0];
      pick.busy = true;
      pick.last_seen = Date.now();
      this.peers.set(pick.peer_id, pick);
      await this.state.storage.put("peers", [...this.peers.entries()]);

      const ttl = parseInt(this.env.ACCESS_TOKEN_TTL_SEC || "3600", 10);
      const access_token = await this.issueAccessToken(pick.peer_id, ttl);

      return json({
        tunnel_host: pick.tunnel_host,
        access_token,
        local_port: 50053,
        peer_id: pick.peer_id,
        volunteer_pool_size: this.peers.size,
        ttl_sec: ttl,
      });
    }

    return json({ error: "not found" }, 404);
  }

  private async loadPeers() {
    const stored = await this.state.storage.get<[string, PeerRecord]>("peers");
    if (stored) this.peers = new Map(stored);
  }

  /** Cloudflare Access service token — stub issues signed placeholder; replace with CF API in prod. */
  private async issueAccessToken(peerId: string, ttlSec: number): Promise<string> {
    const payload = JSON.stringify({ sub: peerId, exp: Date.now() + ttlSec * 1000 });
    const data = new TextEncoder().encode(payload);
    const digest = await crypto.subtle.digest("SHA-256", data);
    const hex = [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
    return `k3.${peerId}.${hex.slice(0, 32)}`;
  }
}

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "Content-Type": "application/json", "Access-Control-Allow-Origin": "*" },
  });
}
