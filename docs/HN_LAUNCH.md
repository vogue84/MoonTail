# HN launch — free Kimi K3 swarm

Public reciprocal swarm: **no Zero Trust**, **no Tailscale**, **no central GPU**.

| Role | Needs |
|------|-------|
| **Prompter / volunteer** | `moontail`, `curl`, Kimi K3 GGUF (`MOONTAIL_MODEL`) |
| **Volunteer GPU** | above + `cloudflared` + one-time `scripts/volunteer-tunnel.sh` |
| **Operator** | [MoontailAI](https://github.com/rockybalboan19/MoontailAI) on Cloudflare Workers (free tier) |

## Flow

1. Volunteers: `rpc-server` @ `127.0.0.1` + Cloudflare Tunnel TCP → `POST /register`
2. Prompter: `POST /queue` → `POST /session` → `access_mode: tunnel_only`
3. `llama-cli -rpc volunteer-host:50052 -ot config/tensor-overrides.kimi-k3`

## Operator

- Deploy MoontailAI with `MIN_VOLUNTEERS=1` (demo) or `3` (healthy pool)
- Official URL: [`config/official-worker.url`](../config/official-worker.url)

## HN checklist

See [KIMI_K3_LAUNCH.md](KIMI_K3_LAUNCH.md) and [FAQ.md](FAQ.md) (Petals comparison).
