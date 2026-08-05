# MoonTail on Tailscale

Run the swarm over a **Tailscale mesh** instead of Cloudflare Zero Trust Access. No card, no 50-seat IdP limit — every volunteer and prompter joins the **same tailnet**.

| Layer | Tailscale mode |
|-------|----------------|
| **Registry** | Cloudflare Worker (MoontailAI) — public URL, `TRANSPORT=tailscale` |
| **Expert RPC** | Direct TCP on tailnet via `tailscale serve` → `127.0.0.1:50052` |
| **Auth** | Tailnet membership (WireGuard) + `peer_token` on registry API |

`rpc-server` still binds **127.0.0.1 only**. `tailscale serve` exposes the port to the tailnet without opening the public internet.

---

## Operator setup

### 1. Worker transport mode

In **MoontailAI** `wrangler.toml` (already default):

```toml
TRANSPORT = "tailscale"
```

Redeploy, or set `TRANSPORT=tailscale` in Cloudflare dashboard → Worker → Settings → Variables.

You do **not** need `CF_ACCESS_CLIENT_ID` / `CF_ACCESS_CLIENT_SECRET` in tailscale mode.

### 2. Create a tailnet

1. Sign up at [tailscale.com](https://tailscale.com) (free personal plan: ~100 devices)
2. Install Tailscale on your machine
3. Invite volunteers via **Admin console → Settings → Users** or share an **auth key** (Settings → Keys)

All swarm members must show **Connected** in the admin console.

### 3. Official worker URL

Already set in `config/official-worker.url`:

```text
https://moontailai.yash-d-sharma-2021.workers.dev
```

---

## Volunteer / prompter setup (every GPU machine)

### 1. Install Tailscale + MoonTail

```bash
# Tailscale: https://tailscale.com/download
# MoonTail:
bash install.sh
export PATH="$HOME/.moontail/bin:$PATH"
```

### 2. Environment

```bash
export MOONTAIL_WORKER=https://moontailai.yash-d-sharma-2021.workers.dev
export MOONTAIL_TRANSPORT=tailscale
# optional — auto-detected via `tailscale ip -4` if omitted:
export MOONTAIL_TAILSCALE_HOST=$(tailscale ip -4)
```

PowerShell:

```powershell
$env:MOONTAIL_WORKER = "https://moontailai.yash-d-sharma-2021.workers.dev"
$env:MOONTAIL_TRANSPORT = "tailscale"
$env:MOONTAIL_TAILSCALE_HOST = (tailscale ip -4)
```

### 3. Join + prompt

```bash
moontail init --accept-terms
moontail status
moontail prompt "Hello Kimi K2"
```

`init` starts:

1. `rpc-server` on `127.0.0.1:50052`
2. `tailscale serve --bg --tcp=50052 tcp://127.0.0.1:50052`
3. Heartbeat `POST /register` with your Tailscale IP / hostname

`prompt` gets a session with `transport: "tailscale"` and runs `llama-cli -rpc 100.x.x.x:50052` over the tailnet.

---

## Verify

```powershell
# Registry
curl.exe -sf "https://moontailai.yash-d-sharma-2021.workers.dev/status"

# Should include "transport":"tailscale"
# After init on one machine:
# "volunteer_pool_size":1 — set MIN_VOLUNTEERS=1 on Worker for solo test
```

From another tailnet machine, test TCP reachability:

```bash
nc -zv 100.x.x.x 50052   # volunteer Tailscale IP
```

---

## Security notes

- Only machines **on your tailnet** can reach expert RPC ports.
- Control-plane API (`/register`, `/queue`) is still public HTTPS — protected by `peer_token`.
- Do not bind `rpc-server` to `0.0.0.0`.
- Tailscale mode is for **closed/community** swarms you invite — not anonymous open internet volunteers without tailnet access.

---

## Switch back to Cloudflare Access

Set on the Worker:

```toml
TRANSPORT = "cloudflare"
```

Add `CF_ACCESS_CLIENT_ID` / `CF_ACCESS_CLIENT_SECRET` secrets. Volunteers use `MOONTAIL_TRANSPORT=cloudflare` (or unset) + `cloudflared tunnel`.
