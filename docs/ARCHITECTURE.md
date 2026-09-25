# Architecture — index

Canonical reference: **[DEVELOPER.md](DEVELOPER.md)**

- **Model:** [moonshotai/Kimi-K3](https://huggingface.co/moonshotai/Kimi-K3) (`kimi-k3`, llama.cpp)
- **Inference:** `-ot` routed experts → ggml-rpc; backbone local on requester
- **Transport:** `rpc-server` @ `127.0.0.1`; volunteers use outbound Cloudflare Tunnel; sessions `tunnel_only`
- **Control plane:** **MoontailAI** — Worker + Registry DO (`/register`, `/queue`, `/session`, `/status`)
- **Reciprocity:** `moontail setup` before `prompt`

See: [SECURITY.md](SECURITY.md), [BENCHMARKS.md](BENCHMARKS.md), [KIMI_K3_LAUNCH.md](KIMI_K3_LAUNCH.md), [FAQ.md](FAQ.md).
