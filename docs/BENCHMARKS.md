# Benchmark gates (v17)

MoonTail ships **measurement scripts** under `tools/`; results go to `results/`. No marketing tok/s without Gate 4 numbers on your hardware.

## Gate 3 — dependency graph

**Script:** `tools/gate3_depgraph.py`

```bash
python tools/gate3_depgraph.py config/kimi-k3.example.config.json
python tools/gate3_depgraph.py --fetch-k3   # HF metadata only
```

Production target: Kimi K3 (`kimi_k3`, 93 layers, 896 experts).

## Gate 4 — expert RPC localhost

**Script:** `tools/gate4_expert_rpc.sh`

```bash
MODEL=/path/to/k3.gguf bash tools/gate4_expert_rpc.sh
```

Uses `config/tensor-overrides.kimi-k3`. Output: `results/gate4_expert_rpc.json`, `results/phase0_localhost_rpc.json`.

## Gate 5 / 6

- **Gate 5:** pairing-limited concurrency (`tools/gate5_concurrent.sh`)
- **Gate 6:** tunnel path / no public rpc bind (`tools/gate6_auth_tunnel.sh`)

## Phase 0 go/no-go (Kimi K3)

**Decision:** Gate 4 pass on K3 GGUF before public swarm expansion.

See [KIMI_K3_LAUNCH.md](KIMI_K3_LAUNCH.md).

| Artifact | Role |
|----------|------|
| `config/kimi-k3.example.config.json` | Gate 3 input |
| `config/tensor-overrides.kimi-k3` | `-ot` patterns |
