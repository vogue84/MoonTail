#!/usr/bin/env bash
# Gate 4: expert RPC latency — localhost rpc-server only (dev safe)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODEL="${MODEL:-}"
LLAMA_BENCH="${LLAMA_BENCH:-$ROOT/vendor/llama.cpp/build/bin/llama-bench}"
RPC="${RPC:-127.0.0.1:50052}"
OUT="$ROOT/results/gate4_expert_rpc.json"
mkdir -p "$ROOT/results"

if [[ -z "$MODEL" ]]; then
  echo '{"gate":4,"error":"MODEL not set"}' > "$OUT"
  exit 0
fi

"$LLAMA_BENCH" -m "$MODEL" -rpc "$RPC" \
  -ot 'blk\.*\.ffn_gate_exps=RPC0' \
  -ot 'blk\.*\.ffn_up_exps=RPC0' \
  -ot 'blk\.*\.ffn_down_exps=RPC0' \
  -o json -n 64 2>/dev/null | tee "$OUT" || {
  echo "{\"gate\":4,\"error\":\"bench failed\",\"rpc\":\"$RPC\"}" > "$OUT"
}
echo "Wrote $OUT"
