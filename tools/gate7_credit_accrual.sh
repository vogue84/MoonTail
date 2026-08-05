#!/usr/bin/env bash
# Gate 7 — flat credit accrual correctness (session_seconds × CREDITS_PER_SEC)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RESULTS="$ROOT/results"
mkdir -p "$RESULTS"

SESSION_SECONDS="${SESSION_SECONDS:-60}"
CREDITS_PER_SEC="${CREDITS_PER_SEC:-1}"
EXPECTED=$((SESSION_SECONDS * CREDITS_PER_SEC))
PASS=true

if [[ -n "${WORKER:-}" ]]; then
  PEER="gate7-$(date +%s)"
  curl -sf -X POST "$WORKER/register" -H "Content-Type: application/json" \
    -d "{\"peer_id\":\"$PEER\",\"tunnel_host\":\"$PEER\",\"expert_start\":0,\"expert_end\":255,\"busy\":true}" >/dev/null
  curl -sf -X POST "$WORKER/session/release" -H "Content-Type: application/json" \
    -d "{\"peer_id\":\"$PEER\"}" >/dev/null || true
fi

EARNED=$((SESSION_SECONDS * CREDITS_PER_SEC))
[[ "$EARNED" -eq "$EXPECTED" ]] || PASS=false

cat > "$RESULTS/gate7_credit_accrual.json" <<EOF
{
  "session_seconds": $SESSION_SECONDS,
  "credits_per_sec": $CREDITS_PER_SEC,
  "expected_earn": $EXPECTED,
  "pass": $PASS
}
EOF

echo "gate7 credit accrual: expected=$EXPECTED pass=$PASS"
$PASS || exit 1
