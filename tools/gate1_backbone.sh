#!/usr/bin/env bash
# Gate 1: local backbone timing — experts on CPU (no RPC)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODEL="${MODEL:-}"
LLAMA_BENCH="${LLAMA_BENCH:-$ROOT/vendor/llama.cpp/build/bin/llama-bench}"
OUT="$ROOT/results/gate1_backbone.json"
mkdir -p "$ROOT/results"

if [[ -z "$MODEL" ]]; then
  echo "Set MODEL=path/to/model.gguf" >&2
  exit 1
fi
if [[ ! -x "$LLAMA_BENCH" ]]; then
  echo "Build llama-bench first: cmake -DGGML_RPC=ON .. in vendor/llama.cpp/build" >&2
  exit 1
fi

"$LLAMA_BENCH" -m "$MODEL" \
  -ot 'blk\.*\.ffn_gate_exps=CPU' \
  -ot 'blk\.*\.ffn_up_exps=CPU' \
  -ot 'blk\.*\.ffn_down_exps=CPU' \
  -o json -n 128 -ngl 99 2>/dev/null | tee "$OUT" || {
  echo '{"error":"bench failed","gate":1}' > "$OUT"
  exit 1
}
echo "Wrote $OUT"
