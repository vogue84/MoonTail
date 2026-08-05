export interface Env {
  REGISTRY: DurableObjectNamespace;
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

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    const id = env.REGISTRY.idFromName("global");
    const stub = env.REGISTRY.get(id);
    const path = url.pathname;

    if (path === "/health" || path === "/status") {
      const stats = await stub.fetch(new Request(`https://do${path === "/health" ? "/stats" : "/status"}`));
      return new Response(await stats.text(), { headers: { "Content-Type": "application/json" } });
    }

    const proxy = async (doPath: string, method: string, body?: string) => {
      const headers = new Headers(request.headers);
      const ip = request.headers.get("CF-Connecting-IP");
      if (ip) headers.set("CF-Connecting-IP", ip);
      return stub.fetch(new Request(`https://do${doPath}`, { method, body, headers }));
    };

    if (path === "/register" && request.method === "POST") {
      return proxy("/register", "POST", await request.text());
    }
    if (path === "/deregister" && request.method === "POST") {
      return proxy("/deregister", "POST", await request.text());
    }
    if (path === "/session" && request.method === "POST") {
      return proxy("/session", "POST", await request.text());
    }
    if (path === "/session/release" && request.method === "POST") {
      return proxy("/session/release", "POST", await request.text());
    }
    if (path === "/queue" && request.method === "POST") {
      return proxy("/queue", "POST", await request.text());
    }
    if (path.startsWith("/queue/") && request.method === "GET") {
      return proxy(path, "GET");
    }

    return new Response(JSON.stringify({ error: "not found" }), {
      status: 404,
      headers: { "Content-Type": "application/json" },
    });
  },
};

export { Registry } from "./registry";
