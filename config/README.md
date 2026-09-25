# Config

| File | Purpose |
|------|---------|
| `kimi-k3.example.config.json` | Kimi K3 Gate 3 reference |
| `tensor-overrides.kimi-k3` | `-ot` expert → RPC0 patterns |
| `hf-kimi-k3-meta/` | HF metadata snapshots |
| `depgraph-templates.json` | Gate 3 layer graph (`kimi_k3`) |
| `official-worker.url` | Public registry URL |
| `cloudflared-volunteer.yml` | Example tunnel (override in `~/.moontail/`) |
| `MIN_LLAMA_VERSION` | Security pin |

```bash
python tools/gate3_depgraph.py config/kimi-k3.example.config.json
```
