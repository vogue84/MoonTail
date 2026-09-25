#!/usr/bin/env bash
# MoonTail does not host weights — set MOONTAIL_MODEL (see docs/KIMI_K3_LAUNCH.md).
set -euo pipefail
if [[ -n "${MOONTAIL_MODEL:-}" ]]; then echo "model=$MOONTAIL_MODEL"; exit 0; fi
if [[ -f "${HOME:-}/.moontail/k3.gguf" ]]; then echo "model=${HOME}/.moontail/k3.gguf"; exit 0; fi
echo "MoonTail: download or convert Kimi K3 GGUF yourself." >&2
exit 1
