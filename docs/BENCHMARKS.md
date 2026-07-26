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

Default input: `config/kimi-linear-48b-proxy.config.json` (Kimi-Linear-48B-A3B **proxy**, 27 layers — tooling dry-run only, **not** Kimi K3 2.8T).

Output: `results/gate3.json` (and `results/gate3_depgraph.json` alias).

Required fields when using proxy config:

```json
{
  "verdict_status": "provisional-on-proxy-config",
  "trust_verdict": false,
  "checkpoint_target": "Kimi K3 (2.8T) — NOT answered by this proxy config",
  "architecture_template": "kimi_linear"
}
```

Re-run with real K3 `config.json` when weights land; expect `architecture_template: kimi_k3` and Block AttnRes in layer graph. Do **not** treat proxy verdict as K3's answer.

## Gate 4 — Expert RPC latency (localhost dev)

**Script:** `tools/gate4_expert_rpc.sh`

Requires `rpc-server -H 127.0.0.1` on localhost. Output: `results/gate4_expert_rpc.json`.

## Gate 5 — Concurrent streams (pairing ceiling)

**Script:** `tools/gate5_concurrent.sh`

**Assumption:** B concurrent streams requires B paired volunteers (one session each). A ceiling below B is a **pairing-model limit**, not a compute/VRAM shortage.

Required fields:

```json
{
  "requested_B": 50,
  "volunteer_pool_size": 10,
  "achieved_concurrent": 10,
  "pairing_limited": true
}
```

## Gate 6 — Auth tunnel connectivity

**Script:** `tools/gate6_auth_tunnel.sh`

Measures session + Cloudflare Tunnel + Access path only. Direct RPC port exposure = automatic fail.

Output: `results/gate6_auth_tunnel.json`.
