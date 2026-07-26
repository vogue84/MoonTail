#!/usr/bin/env bash
# Gate 5: concurrent streams — PAIRING CEILING documented
# ASSUMPTION: B concurrent streams requires B paired volunteers (1 session each).
# A ceiling below B is a PAIRING-MODEL limit, not a compute/VRAM shortage.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORKER="${WORKER:-http://127.0.0.1:8787}"
B="${B:-5}"
OUT="$ROOT/results/gate5_concurrent.json"
mkdir -p "$ROOT/results"

volunteer_pool=$(curl -sf "$WORKER/health" 2>/dev/null | grep -o 'volunteer_pool_size":[0-9]*' | cut -d: -f2 || echo "0")
achieved=0
for ((i=0; i<B; i++)); do
  if curl -sf -X POST "$WORKER/session" -H "Content-Type: application/json" \
      -d '{"expert_start":0,"expert_end":255}' >/dev/null 2>&1; then
    achieved=$((achieved + 1))
  else
    break
  fi
done

pairing_limited="false"
[[ "$achieved" -lt "$B" ]] && pairing_limited="true"

cat > "$OUT" <<EOF
{
  "gate": 5,
  "requested_B": $B,
  "volunteer_pool_size": ${volunteer_pool:-0},
  "achieved_concurrent": $achieved,
  "pairing_limited": ${pairing_limited}
}
EOF
echo "Wrote $OUT"
