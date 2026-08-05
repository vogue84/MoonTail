# Measurement Gates — v17

All gates write JSON under `results/`. Run after building llama.cpp (`vendor/llama.cpp/build/bin/`).

## Gate 1 — Local backbone per verification round

**Script:** `tools/gate1_backbone.sh`

Measures llama-bench with expert tensors on CPU (backbone + router + shared expert local). Output: `results/gate1_backbone.json`.

## Gate 2 — Speculative acceptance rate

**Script:** `tools/gate2_speculative.sh`

Uses llama-server with `--spec-type` variants. Output: `results/gate2_speculative.json`.

## Gate 3 — Checkpoint dependency graph

**Script:** `tools/gate3_depgraph.py`

Production target: `config/kimi-k2.example.config.json` (Kimi K2-Instruct, 61 layers, 384 experts, `deepseek2`).

Default dry-run: `config/kimi-linear-48b-proxy.config.json` (27 layers — proxy only).

```bash
python tools/gate3_depgraph.py config/kimi-k2.example.config.json
python tools/gate3_depgraph.py --fetch-k2   # HF metadata only
```

## Gate 4 — Expert RPC latency (localhost dev)

**Script:** `tools/gate4_expert_rpc.sh`

Uses `config/tensor-overrides.kimi-k2`. Output: `results/gate4_expert_rpc.json` and `results/phase0_localhost_rpc.json`.

## Gate 5 — Concurrent streams (pairing ceiling)

**Script:** `tools/gate5_concurrent.sh`

## Gate 6 — Auth tunnel connectivity

**Script:** `tools/gate6_auth_tunnel.sh`

Sessions require Cloudflare Access in production (no stub mode). Raw rpc-server on 0.0.0.0 = fail.

## Phase 0 go/no-go (Kimi K2)

**Decision: GO** on upstream llama.cpp master with `deepseek2` + `-ot` expert offload.

See `docs/KIMI_K2_LAUNCH.md` for deploy checklist.

| Artifact | Purpose |
|----------|---------|
| `config/kimi-k2.example.config.json` | 61 layers, 384 experts |
| `config/tensor-overrides.kimi-k2` | GGUF `-ot` patterns |
| `results/phase0_localhost_rpc.json` | Localhost RPC transport |

## Gate 7 — Credit accrual

**Script:** `tools/gate7_credit_accrual.sh` — optional; `FEATURE_CREDITS=0` by default.
