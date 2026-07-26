# Architecture — Reciprocal MoE Swarm v17 (thin)

- **Inference:** llama.cpp submodule (`-ot` expert tensors → RPC, backbone local)
- **Transport:** ggml-rpc on `127.0.0.1` only; cross-machine via Cloudflare Tunnel + Access
- **Control plane:** Worker + Registry Durable Object (`/register`, `/session`)
- **Client:** `swarm-cli` — session, cloudflared access tcp, exec llama
- **Volunteer:** `volunteer.c` — supervise rpc-server + cloudflared

See the v17 plan for gates, pairing ceiling, and security invariants.
