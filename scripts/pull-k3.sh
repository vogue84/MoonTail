#!/usr/bin/env bash
# Kimi K3 weights — MoonTail does not host models. Options:
# 1) Community GGUF (e.g. unsloth/Kimi-K3-GGUF on Hugging Face) → set MOONTAIL_MODEL path
# 2) Convert official weights with upstream llama.cpp:
#    cd vendor/llama.cpp && python conversion/kimi_k3.py --help
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
echo "MoonTail: download or convert Kimi K3 GGUF yourself (see docs/KIMI_K3_LAUNCH.md)." >&2
if [[ -n "${MOONTAIL_MODEL:-}" ]]; then
  echo "model=$MOONTAIL_MODEL"
  exit 0
fi
if [[ -f "$HOME/.moontail/k3.gguf" ]]; then
  echo "model=$HOME/.moontail/k3.gguf"
  exit 0
fi
exit 1
