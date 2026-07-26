# Security Policy — Reciprocal MoE Swarm v17

## ggml-rpc is not a public API

llama.cpp's RPC backend is proof-of-concept, fragile, and **must not** be exposed to untrusted networks. See upstream [`SECURITY.md`](../vendor/llama.cpp/SECURITY.md) and [`tools/rpc/README.md`](../vendor/llama.cpp/tools/rpc/README.md).

KimiK3 enforces:

1. **`rpc-server` binds `127.0.0.1` only** — never `0.0.0.0`, never port-forward to the public internet.
2. **Cross-machine traffic uses Cloudflare Tunnel + Access only** — session-scoped credentials from the control plane.
3. **One active ggml-rpc client per volunteer `rpc-server`** (v1 pairing model).
4. **`--skip-tunnel` is client-only (swarm-cli)** for localhost dev; refused when `--worker` is not localhost. **volunteer.c has no tunnel bypass.**

## Version pin (security control)

> **The llama.cpp / ggml-rpc version pin is a security control, not a version preference.** Any ggml-rpc CVE advisory triggers a same-day forced submodule bump and volunteer/client redeploy — triaged as a security patch, not scheduled like a feature upgrade.

Minimum submodule commit: **b8492** or later (CVE-2026-34159 patched). Enforced by `scripts/check-security.sh`.

## Residual risks

| Risk | Direction | Mitigation |
|------|-----------|------------|
| Authenticated requester → volunteer RCE | Client harms server | Tunnel limits unauthenticated exposure; **pin >= b8492 + same-day CVE bumps** address authenticated deserialization bugs (CVE-2026-34159 class). |
| Malicious volunteer → bad tensors | Server harms client | Reputation / shard config; no cryptographic verify in v1. |
| ggml-rpc PoC fragility | Both | Localhost bind + version pin + upstream monitoring. |

## Operator checklist

- [ ] `check-security.sh` passes before deploy
- [ ] Volunteer runs outbound `cloudflared tunnel` only (no inbound RPC port)
- [ ] Access token TTL ≤ session length + 5 minutes
- [ ] No `/peers` or tunnel hostname without successful `POST /session`
- [ ] `--skip-tunnel` never used with non-localhost `--worker`
- [ ] `volunteer.c` always runs cloudflared + rpc-server on 127.0.0.1
