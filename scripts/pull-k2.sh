#!/usr/bin/env bash
# Stream-convert Kimi K2-Instruct from HF. Needs HF_TOKEN + Python + upstream llama.cpp.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CACHE="${MOONTAIL_CACHE:-$HOME/.moontail}"
MODEL="$CACHE/kimi-k2.gguf"
mkdir -p "$CACHE"
if [[ -f "$MODEL" ]]; then
  echo "model=$MODEL"
  exit 0
fi
if [[ -z "${HF_TOKEN:-}" ]]; then
  echo "Set HF_TOKEN (accept moonshotai/Kimi-K2-Instruct on Hugging Face)" >&2
  exit 1
fi
PY="${MOONTAIL_PYTHON:-python3}"
CONVERT="$ROOT/vendor/llama.cpp/convert_hf_to_gguf.py"
if [[ ! -f "$CONVERT" ]]; then
  echo "Missing $CONVERT — run install.sh first" >&2
  exit 1
fi
echo "Pulling Kimi K2-Instruct from Hugging Face into $MODEL (streamed, may take hours)..." >&2
"$PY" "$CONVERT" --remote moonshotai/Kimi-K2-Instruct --outfile "$MODEL" --outtype auto
echo "model=$MODEL"
