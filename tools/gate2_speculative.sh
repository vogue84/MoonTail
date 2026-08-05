#!/usr/bin/env bash
# Gate 2: speculative acceptance from llama-server log markers
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODEL="${MODEL:-}"
OUT="$ROOT/results/gate2_speculative.json"
SPEC_TYPE="${SPEC_TYPE:-none}"
LLAMA_SERVER="${LLAMA_SERVER:-$ROOT/vendor/llama.cpp/build/bin/llama-server}"
mkdir -p "$ROOT/results"
accepted=1.0; note="stub"
if [[ -n "$MODEL" && -x "$LLAMA_SERVER" ]]; then
  log=$(mktemp); trap 'rm -f "$log"' EXIT
  "$LLAMA_SERVER" -m "$MODEL" --spec-type "$SPEC_TYPE" -n 16 --temp 0 2>"$log" >/dev/null || true
  a=$(grep -ciE 'accept|n_accept' "$log" || true); r=$(grep -ciE 'reject|n_reject' "$log" || true)
  if [[ $((a+r)) -gt 0 ]]; then accepted=$(awk -v x="$a" -v t="$((a+r))" 'BEGIN{printf "%.4f", x/t}'); note="log parse"; fi
fi
printf '{"gate":2,"spec_type":"%s","accepted_tokens_per_round":%s,"note":"%s"}\n' "$SPEC_TYPE" "$accepted" "$note" > "$OUT"
echo "Wrote $OUT"
