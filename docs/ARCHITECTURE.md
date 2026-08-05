# Architecture — index

Canonical reference: **[DEVELOPER.md](DEVELOPER.md)**

- **Model:** Kimi K2-Instruct (`deepseek2`, upstream llama.cpp master)
- **Inference:** llama.cpp `-ot` expert tensors → RPC; backbone local
- **Transport:** ggml-rpc on `127.0.0.1` only; cross-machine via Cloudflare Tunnel + Access
- **Control plane:** Worker + Registry DO (`/register`, `/queue`, `/session`, `/status`)
- **Reciprocity:** volunteer (`init --accept-terms`) before prompt

See: [SECURITY.md](SECURITY.md), [BENCHMARKS.md](BENCHMARKS.md), [KIMI_K2_LAUNCH.md](KIMI_K2_LAUNCH.md).
