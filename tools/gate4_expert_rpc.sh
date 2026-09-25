#!/usr/bin/env bash
# Gate 4 + Phase 0c: localhost rpc-server + llama-bench -ot
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODEL="${MODEL:-}"
BENCH="${LLAMA_BENCH:-$ROOT/vendor/llama.cpp/build/bin/llama-bench}"
RPC_BIN="${RPC_BIN:-$ROOT/vendor/llama.cpp/build/bin/rpc-server}"
RPC="${RPC:-127.0.0.1:50052}"
OUT="$ROOT/results/gate4_expert_rpc.json"
P0="$ROOT/results/phase0_localhost_rpc.json"
OVERRIDES="$ROOT/config/tensor-overrides.kimi-k3"
mkdir -p "$ROOT/results"

write_p0() { echo "$1" > "$P0"; }

if [[ -z "$MODEL" ]]; then
  echo '{"gate":4,"error":"MODEL not set"}' > "$OUT"
  write_p0 '{"phase":"0c","pass":false,"error":"MODEL not set"}'
  exit 0
fi

if [[ ! -x "$BENCH" || ! -x "$RPC_BIN" ]]; then
  echo '{"gate":4,"error":"build rpc-server and llama-bench with -DGGML_RPC=ON"}' > "$OUT"
  write_p0 '{"phase":"0c","pass":false,"error":"GGML_RPC build missing"}'
  exit 0
fi

host="${RPC%%:*}"; port="${RPC##*:}"
"$RPC_BIN" -H "$host" -p "$port" -m "$MODEL" &
rpc_pid=$!
sleep 2
ot=()
while IFS= read -r line; do [[ "$line" =~ ^-ot ]] && ot+=("$line"); done < <(grep -v '^#' "$OVERRIDES" || true)

if "$BENCH" -m "$MODEL" -rpc "$RPC" "${ot[@]}" -o json -n 64 2>/dev/null | tee "$OUT"; then
  write_p0 "{\"phase\":\"0c\",\"transport_ok\":true,\"bench_ok\":true,\"pass\":true,\"rpc\":\"$RPC\"}"
else
  echo "{\"gate\":4,\"error\":\"bench failed\",\"rpc\":\"$RPC\"}" > "$OUT"
  write_p0 "{\"phase\":\"0c\",\"transport_ok\":true,\"bench_ok\":false,\"pass\":false}"
fi
kill "$rpc_pid" 2>/dev/null || true
echo "Wrote $OUT and $P0"
