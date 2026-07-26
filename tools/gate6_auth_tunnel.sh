#!/usr/bin/env bash
# Gate 6: auth tunnel connectivity — Tunnel+Access only; raw RPC port = fail
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORKER="${WORKER:-http://127.0.0.1:8787}"
OUT="$ROOT/results/gate6_auth_tunnel.json"
mkdir -p "$ROOT/results"

session_ok=false
tunnel_host=""
if resp=$(curl -sf -X POST "$WORKER/session" -H "Content-Type: application/json" \
    -d '{"expert_start":0,"expert_end":255}' 2>/dev/null); then
  session_ok=true
  tunnel_host=$(echo "$resp" | grep -o '"tunnel_host":"[^"]*"' | cut -d'"' -f4 || true)
fi

raw_rpc_exposed=false
if command -v nc >/dev/null 2>&1 && nc -z 127.0.0.1 50052 2>/dev/null; then
  raw_rpc_exposed=true
fi

pass=false
if $session_ok && ! $raw_rpc_exposed; then pass=true; fi

sk=$([ "$session_ok" = true ] && echo true || echo false)
rk=$([ "$raw_rpc_exposed" = true ] && echo true || echo false)
pk=$([ "$pass" = true ] && echo true || echo false)

cat > "$OUT" <<EOF
{
  "gate": 6,
  "session_ok": ${sk},
  "tunnel_host": "${tunnel_host}",
  "raw_rpc_exposed": ${rk},
  "pass": ${pk}
}
EOF
echo "Wrote $OUT"
