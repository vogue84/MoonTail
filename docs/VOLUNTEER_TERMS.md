# Volunteer terms (GPU lending)

By running `moontail init --accept-terms` you agree:

1. **You lend GPU cycles** on your machine to run routed expert FFN for other swarm members' inference jobs.
2. **Electricity and hardware wear** are your responsibility.
3. **ggml-rpc is upstream proof-of-concept** software. MoonTail binds rpc-server to `127.0.0.1` and uses Cloudflare Tunnel + Access for cross-machine traffic only.
4. **Prompts and model outputs are opaque compute payloads.** MoonTail operators do not review, filter, or log content for moderation purposes.
5. **No warranty.** Research preview software — use at your own risk.
6. **Moonshot license** applies to Kimi K2 weights you download from Hugging Face.

To stop volunteering: kill `moontail-volunteer` and remove your peer from the pool (stale peers evict after ~120s without heartbeat).

Abuse of the control plane (spam register/queue) may be rate-limited or blocked.
