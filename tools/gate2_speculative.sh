#!/usr/bin/env bash
# Gate 2: speculative acceptance — llama-server log parse (stub metrics)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODEL="${MODEL:-}"
LLAMA_SERVER="${LLAMA_SERVER:-$ROOT/vendor/llama.cpp/build/bin/llama-server}"
OUT="$ROOT/results/gate2_speculative.json"
SPEC_TYPE="${SPEC_TYPE:-none}"
mkdir -p "$ROOT/results"

if [[ -z "$MODEL" ]]; then
  echo '{"gate":2,"error":"MODEL not set","accepted_tokens_per_round":1.0}' > "$OUT"
  exit 0
fi

# Placeholder: run brief generation; production parses server logs for accept/reject counts
cat > "$OUT" <<EOF
{
  "gate": 2,
  "spec_type": "$SPEC_TYPE",
  "accepted_tokens_per_round": 1.0,
  "note": "Set MODEL and run llama-server --spec-type draft; parse logs for real acceptance rate"
}
EOF
echo "Wrote $OUT (stub — set MODEL for live run)"
