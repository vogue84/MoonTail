# End-to-end swarm test

Prerequisites: K3 GGUF on two machines (or one machine + `MIN_VOLUNTEERS=1`), MoontailAI deployed, official worker URL live.

```bash
curl -sf "$(grep -v '^#' config/official-worker.url | head -1)/status"
```

## Machine A (volunteer)

```bash
bash install.sh
export PATH="$HOME/.moontail/bin:$PATH"
export MOONTAIL_MODEL=/path/to/k3.gguf
bash scripts/volunteer-tunnel.sh
moontail setup --accept-terms
```

## Machine B (prompter, also volunteer)

Same as A — reciprocity requires registration before prompt.

```bash
moontail prompt "Say hello in one sentence."
```

Expect: queue → session with `access_mode: tunnel_only` → llama-cli output (may be very slow).

## Failure modes

| Symptom | Check |
|---------|--------|
| `swarm not ready` | `/status` `waiting_for`, more volunteers |
| `session failed` | volunteer tunnel up, `tunnel_host` in register |
| `bad session` | Worker returns `rpc_host` + `rpc_port` |
| llama error | `MOONTAIL_MODEL`, llama.cpp K3 build, `-ot` patterns |
