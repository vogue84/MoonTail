# Config files

| File | Purpose |
|------|---------|
| `kimi-linear-48b-proxy.config.json` | **Default Gate 3 dry-run** — Kimi-Linear-48B-A3B stand-in (27 layers). Provisional only. |
| `config.json` | Deprecated alias of proxy config; use `kimi-linear-48b-proxy.config.json`. |
| `tensor-overrides.k3` | llama.cpp `-ot` patterns for expert RPC offload. |
| `MIN_LLAMA_VERSION` | Security pin (b8492+). |

When real **Kimi K3** weights land, add `kimi-k3.config.json` from HF and run:

```bash
python tools/gate3_depgraph.py config/kimi-k3.config.json
```

Expect `architecture_template: kimi_k3` and Block AttnRes in the dependency graph.
