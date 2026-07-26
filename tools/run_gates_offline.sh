#!/usr/bin/env bash
# Run all gates that do not require MODEL or live worker
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p results
python3 tools/gate3_depgraph.py
bash tools/gate2_speculative.sh
echo "Optional: MODEL=... bash tools/gate1_backbone.sh"
echo "Optional: MODEL=... bash tools/gate4_expert_rpc.sh (localhost rpc-server)"
echo "Optional: WORKER=... bash tools/gate5_concurrent.sh"
echo "Optional: WORKER=... bash tools/gate6_auth_tunnel.sh"
