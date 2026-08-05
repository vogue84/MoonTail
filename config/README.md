# Config files

| File | Purpose |
|------|---------|
| `kimi-k2.example.config.json` | Kimi K2-Instruct (61 layers, 384 experts, deepseek2) |
| `kimi-linear-48b-proxy.config.json` | Gate 3 dry-run proxy (27 layers) |
| `tensor-overrides.kimi-k2` | `-ot` patterns for K2 routed experts → RPC0 |
| `shard-manifest.example.json` | Expert shard ranges for volunteers |
| `cloudflare-access-policy.example.json` | Zero Trust deny-by-default template |
| `MIN_LLAMA_VERSION` | Security pin (b8492+) |
| `features.json` | `FEATURE_CREDITS` documentation mirror |

```bash
python tools/gate3_depgraph.py config/kimi-k2.example.config.json
```
