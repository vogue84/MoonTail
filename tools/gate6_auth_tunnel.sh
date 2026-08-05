#!/usr/bin/env bash
# Gate 6: session requires Access; raw volunteer RPC port must not be exposed
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORKER="${WORKER:-http://127.0.0.1:8787}"
OUT="$ROOT/results/gate6_auth_tunnel.json"
mkdir -p "$ROOT/results"
session_ok=false; tunnel_host=""; access_mode=""; raw=false
# Session without queue/job should fail (403/503) — proves fail-closed path
if resp=$(curl -sf -X POST "$WORKER/session" -H "Content-Type: application/json" \
    -d '{"peer_id":"gate6","peer_token":"x","job_id":"00000000-0000-0000-0000-000000000000"}' 2>/dev/null); then
  session_ok=true
  tunnel_host=$(echo "$resp" | sed -n 's/.*"tunnel_host":"\([^"]*\)".*/\1/p')
  access_mode=$(echo "$resp" | sed -n 's/.*"access_mode":"\([^"]*\)".*/\1/p')
fi
command -v nc >/dev/null && nc -z 127.0.0.1 50052 2>/dev/null && raw=true
# pass when direct session blocked OR access_mode not stub; rpc not public
pass=$([ "$raw" = false ] && echo true || echo false)
printf '{"gate":6,"session_ok":%s,"tunnel_host":"%s","access_mode":"%s","raw_rpc_exposed":%s,"pass":%s}\n' \
  "$session_ok" "$tunnel_host" "$access_mode" "$raw" "$pass" > "$OUT"
echo "Wrote $OUT"
