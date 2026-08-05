# MoonTail server hosting (maintainers only)

**The MoonTail control plane is operated by the project maintainers only.**

Users join the **official** swarm via `MOONTAIL_WORKER` from [`config/official-worker.url`](../config/official-worker.url). Do not publish self-hosting instructions to end users — there is one community swarm.

---

## Deploy (maintainers)

### 1. Prerequisites

```bash
npm install -g wrangler
wrangler login
```

Cloudflare account with Workers + Zero Trust Access enabled.

### 2. Deploy Worker

```bash
cd server/control-plane
npm install
npx wrangler deploy
```

Copy the deployed URL into **`config/official-worker.url`** (single `https://...` line) and commit so `install.sh` / README stay in sync.

### 3. Access secrets (required for prompts)

```bash
npx wrangler secret put CF_ACCESS_CLIENT_ID
npx wrangler secret put CF_ACCESS_CLIENT_SECRET
```

Create service token in Zero Trust → Access → Service Auth. Policy template: [`config/cloudflare-access-policy.example.json`](../config/cloudflare-access-policy.example.json)

### 4. Tune swarm

Edit [`wrangler.toml`](../server/control-plane/wrangler.toml): `MIN_VOLUNTEERS`, `MAX_CONCURRENT_PROMPTS`, redeploy.

### 5. Verify

```bash
curl -sf "$(grep -v '^#' config/official-worker.url | grep -v '^$' | head -1)/status"
```

See also [KIMI_K2_LAUNCH.md](KIMI_K2_LAUNCH.md).
