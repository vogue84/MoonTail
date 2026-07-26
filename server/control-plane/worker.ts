export interface Env {
  REGISTRY: DurableObjectNamespace;
  ACCESS_TOKEN_TTL_SEC?: string;
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    const id = env.REGISTRY.idFromName("global");
    const stub = env.REGISTRY.get(id);
    const path = url.pathname;

    if (path === "/health") {
      return new Response(JSON.stringify({ ok: true }), {
        headers: { "Content-Type": "application/json" },
      });
    }

    if (path === "/register" && request.method === "POST") {
      return stub.fetch(new Request("https://do/register", {
        method: "POST",
        body: await request.text(),
        headers: request.headers,
      }));
    }

    if (path === "/session" && request.method === "POST") {
      return stub.fetch(new Request("https://do/session", {
        method: "POST",
        body: await request.text(),
        headers: request.headers,
      }));
    }

    if (path === "/peers" && request.method === "GET") {
      return new Response(JSON.stringify({ error: "use POST /session first" }), {
        status: 403,
        headers: { "Content-Type": "application/json" },
      });
    }

    return new Response(JSON.stringify({ error: "not found" }), {
      status: 404,
      headers: { "Content-Type": "application/json" },
    });
  },
};

export { Registry } from "./registry";
