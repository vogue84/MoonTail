# Kimi K2 — launch checklist

**Target:** `moonshotai/Kimi-K2-Instruct` on upstream llama.cpp `master` (`deepseek2`, 61 layers, 384 experts).

## The deal

1. `moontail init --accept-terms` — volunteer GPU + join swarm
2. Wait until `MIN_VOLUNTEERS` (default 3) peers registered
3. `moontail prompt "…"` — FIFO queue → session → llama-cli with `-ot` expert offload
4. Decent tok/s when queue clears — **measure with gates**, don't market unverified numbers

## Hardware

| Path | Notes |
|------|-------|
| Community Q4 split GGUF | Fastest try-it path; point `-m` at first shard |
| `scripts/pull-k2.sh` | Stream HF convert; hours; needs `HF_TOKEN` |

K2 at Q4 is multi-hundred GB class — plan storage and VRAM honestly in README.

## Control plane (production)

**Maintainers only** — one official MoonTail server. Deploy steps: [HOSTING.md](HOSTING.md). User-facing URL: [`config/official-worker.url`](../config/official-worker.url).

Required Worker secrets:

- `CF_ACCESS_CLIENT_ID` / `CF_ACCESS_CLIENT_SECRET` — **sessions fail closed without these**
- `MIN_VOLUNTEERS`, `MAX_CONCURRENT_PROMPTS`

See [`config/cloudflare-access-policy.example.json`](../config/cloudflare-access-policy.example.json).

## Expert offload

Local: embed, router, shared expert, MLA KV  
Remote (`-ot`): `blk.*.ffn_{gate,up,down}_exps` → RPC0

Config: [`config/tensor-overrides.kimi-k2`](../config/tensor-overrides.kimi-k2)

## Go/no-go

| Check | Pass criteria |
|-------|---------------|
| Submodule | Upstream master, not PR #26185 |
| Gate 4 | localhost RPC + `-ot` bench on proxy or K2 GGUF |
| Gate 6 | No public rpc-server; Access not stub |
| Dream smoke | 3× `init --accept-terms`, 1× `prompt` end-to-end |

## HN post checklist

- Lead with reciprocity (volunteer → queue → K2)
- Label research preview / ggml-rpc PoC
- Link [`docs/VOLUNTEER_TERMS.md`](VOLUNTEER_TERMS.md) and [`docs/SECURITY.md`](SECURITY.md)
- No unverified tok/s claims
