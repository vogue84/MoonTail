# MoonTail FAQ

## How is this different from Petals?

Yes — philosophically this is in the same family as [Petals](https://github.com/bigscience-workshop/petals): **distributed inference where nobody holds the full model**, and people contribute compute so others can run workloads they couldn’t alone.

What’s different on purpose:

| | **Petals (classic)** | **MoonTail (this repo)** |
|---|----------------------|---------------------------|
| **Engine** | Custom PyTorch / Hivemind stack | **[llama.cpp](https://github.com/ggml-org/llama.cpp)** + **ggml-rpc** — we don’t implement MoE math or a new wire protocol |
| **Discovery / routing** | Decentralized P2P / DHT-style swarm | **Small HTTP registry** (Cloudflare Worker) — register, queue, session pairing only |
| **Cross-machine transport** | Direct peer connectivity (research stack) | **Volunteer outbound [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/)**; `rpc-server` stays on **127.0.0.1** |
| **Target model** | e.g. BLOOM-era open models | **Kimi K3** (frontier MoE) via upstream llama.cpp + community GGUF |
| **Scope of our code** | Full research system | **~800 LOC** client/volunteer glue: queue → session → `llama-cli -rpc -ot` |
| **Status** | Mature research project | **Research preview** — ggml-rpc is PoC; expect slow tokens over WAN |

We’re not claiming to replace Petals’ research contributions; we’re **betting on the llama.cpp ecosystem** and a **minimal operator-hosted router** so the stack stays small and reproducible.

**Why not just use Petals for K3?** K3’s graph, quant formats, and tooling live in the llama.cpp line today; MoonTail’s job is **swarm pairing + security boundaries** around that engine.

## Do I run the whole Kimi K3 model on my machine?

**No.** You contribute a **GPU slice** (routed expert weights via `rpc-server`) and/or act as **requester** (local backbone + router + `-ot` expert RPC). The full ~2.8T model exists only **across the swarm**.

## Is the backend free?

**For launch scale, yes.** The operator hosts a **Cloudflare Worker + Durable Object** (routing only — no GPU, no weights). Volunteers use a **free Cloudflare account** for outbound tunnels. You pay for your own power and VRAM.

## Why is inference slow?

WAN RTT × many MoE RPC sync points per token dominates. This is a **research preview**, not a latency-optimized production API.

## How do I get weights?

MoonTail does **not** host GGUF. Use community quants (e.g. Hugging Face `unsloth/Kimi-K3-GGUF`) or convert official weights with upstream `vendor/llama.cpp/conversion/kimi_k3.py`. See [KIMI_K3_LAUNCH.md](KIMI_K3_LAUNCH.md).
