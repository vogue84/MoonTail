# Kimi K3 — swarm launch checklist

**Target:** [moonshotai/Kimi-K3](https://huggingface.co/moonshotai/Kimi-K3) on upstream llama.cpp with `LLM_ARCH_KIMI_K3` (93 layers, 896 experts, 16 experts/token).

## The deal (swarm-only)

1. `moontail setup --accept-terms` — volunteer expert VRAM + join registry
2. Wait until `MIN_VOLUNTEERS` peers registered (`GET /status`)
3. `moontail prompt "…"` — queue → session (`tunnel_only`) → `llama-cli -rpc -ot`
4. Label **research preview**; no unverified tok/s claims

Nobody runs the full model alone — **collective hardware**.

## Weights

| Path | Notes |
|------|-------|
| Community GGUF | e.g. Unsloth quant on Hugging Face — set `MOONTAIL_MODEL` |
| Upstream convert | `vendor/llama.cpp/conversion/kimi_k3.py` from official Safetensors |

Tensor offload: [`config/tensor-overrides.kimi-k3`](../config/tensor-overrides.kimi-k3)

## Operator (MoontailAI — free tier)

Deploy private **MoontailAI** Worker:

- `TRANSPORT=cloudflare` (implicit)
- `MIN_VOLUNTEERS=1` for demo → raise when pool is healthy
- No `CF_ACCESS_*` secrets → `access_mode: tunnel_only`

Public URL: [`config/official-worker.url`](../config/official-worker.url)

## Volunteer

```bash
bash install.sh
bash scripts/volunteer-tunnel.sh
export MOONTAIL_MODEL=/path/to/k3.gguf
moontail setup --accept-terms
```

## Prompter

Same reciprocity rule: must join swarm before `prompt`. Needs `MOONTAIL_MODEL` (backbone GGUF on requester).

## Go/no-go

| Check | Pass criteria |
|-------|---------------|
| llama.cpp | Submodule includes `src/models/kimi-k3.cpp` |
| Gate 3 | `python tools/gate3_depgraph.py config/kimi-k3.example.config.json` |
| Gate 4 | Localhost `rpc-server` + `-ot` bench when `MODEL` set |
| E2E | `setup` → `prompt` one token via official worker |

## HN post

- **Run Kimi K3 on the swarm — free, on your GPU slice**
- Link [FAQ.md](FAQ.md#how-is-this-different-from-petals) (Petals comparison)
- Link [VOLUNTEER_TERMS.md](VOLUNTEER_TERMS.md), [SECURITY.md](SECURITY.md)
