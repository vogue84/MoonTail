#!/usr/bin/env bash
# MoonTail install — builds client + upstream llama.cpp (Kimi K2 / deepseek2) + RPC tools.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> MoonTail install"
if ! command -v git >/dev/null; then echo "need git"; exit 1; fi
if ! command -v curl >/dev/null; then echo "need curl"; exit 1; fi
if ! command -v gcc >/dev/null && ! command -v cc >/dev/null; then echo "need gcc"; exit 1; fi

git submodule update --init vendor/llama.cpp
LLAMA_SHA="$(git -C vendor/llama.cpp rev-parse --short HEAD)"
echo "llama.cpp submodule @ $LLAMA_SHA"

CC="${CC:-gcc}"
mkdir -p build
$CC -O2 -Wall -std=c11 -Iclient -o build/moontail client/moontail.c
$CC -O2 -Wall -std=c11 -Iclient -o build/moontail-volunteer server/volunteer.c

if command -v cmake >/dev/null; then
  echo "==> Building llama.cpp with GGML_RPC"
  cmake -S vendor/llama.cpp -B vendor/llama.cpp/build -DGGML_RPC=ON -DLLAMA_BUILD_TESTS=OFF
  cmake --build vendor/llama.cpp/build --config Release -j "$(nproc 2>/dev/null || echo 2)" \
    --target llama-cli rpc-server llama-bench
  mkdir -p "$HOME/.moontail/bin"
  for b in llama-cli rpc-server llama-bench; do
    if [[ -x vendor/llama.cpp/build/bin/$b ]]; then
      cp vendor/llama.cpp/build/bin/$b "$HOME/.moontail/bin/" 2>/dev/null || true
    fi
  done
fi

mkdir -p "$HOME/.moontail"
if [[ "${MOONTAIL_VERIFY:-}" == "1" ]]; then
  bash scripts/check-security.sh
fi
echo "Installed: build/moontail build/moontail-volunteer"
echo ""
echo "  export PATH=\"\$HOME/.moontail/bin:\$PATH\""
echo "  export HF_TOKEN=hf_...   # optional: stream-convert Kimi K2 from Hugging Face"
echo "  export MOONTAIL_WORKER=https://your-worker.workers.dev"
echo "  moontail init --accept-terms"
echo "  moontail status"
echo "  moontail prompt \"hello\""
