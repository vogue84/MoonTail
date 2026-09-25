# Security Policy — Reciprocal MoE Swarm (Kimi K3 launch)



## ggml-rpc is not a public API



llama.cpp's RPC backend is proof-of-concept, fragile, and **must not** be exposed to untrusted networks. See upstream [`SECURITY.md`](../vendor/llama.cpp/SECURITY.md) and [`tools/rpc/README.md`](../vendor/llama.cpp/tools/rpc/README.md).



KimiK3 / MoonTail enforces:



1. **`rpc-server` binds `127.0.0.1` only** — never `0.0.0.0`, never port-forward to the public internet.

2. **Cross-machine traffic uses Cloudflare Tunnel** — public launch uses **`access_mode: tunnel_only`** (session-gated hostnames). `rpc-server` stays on `127.0.0.1`; never expose raw RPC to the internet.

3. **peer_token** on register, queue, session, release — SHA-256 stored server-side.

4. **Single official control plane** — server is closed source (private **MoontailAI** repo, maintainers only). Users join via `config/official-worker.url`.

5. **One active ggml-rpc client per volunteer `rpc-server`** (v1 pairing model).

6. **`--skip-tunnel` is client-only (moontail)** for localhost dev; refused when worker is not localhost. **volunteer.c has no tunnel bypass.**

7. **Rate limits** on register / queue / session (see registry).



## Version pin (security control)



> **The llama.cpp / ggml-rpc version pin is a security control, not a version preference.** Any ggml-rpc CVE advisory triggers a same-day forced submodule bump and volunteer/client redeploy — triaged as a security patch, not scheduled like a feature upgrade.



Minimum submodule commit: **`config/MIN_LLAMA_VERSION`** (currently **4b1a27fa0**, K3-capable master; includes CVE-2026-34159 fix). Enforced by `scripts/check-security.sh`.



## Residual risks



| Risk | Direction | Mitigation |

|------|-----------|------------|

| Authenticated requester → volunteer RCE | Client harms server | Tunnel limits unauthenticated exposure; **pin >= b8492 + same-day CVE bumps** address authenticated deserialization bugs (CVE-2026-34159 class). |

| Malicious volunteer → bad tensors | Server harms client | Reputation / shard config; no cryptographic verify in v1. |

| ggml-rpc PoC fragility | Both | Localhost bind + version pin + upstream monitoring. |




## Operator checklist



- [ ] `check-security.sh` passes before deploy

- [ ] Volunteer runs outbound `cloudflared tunnel` only (no inbound RPC port)

- [ ] No `/peers` or tunnel hostname without successful `POST /session`

- [ ] `--skip-tunnel` never used with non-localhost `--worker`

- [ ] `volunteer.c` always runs cloudflared + rpc-server on 127.0.0.1

