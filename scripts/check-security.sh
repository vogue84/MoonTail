#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MIN_TAG="$(tr -d ' \r\n' < "$ROOT/config/MIN_LLAMA_VERSION" 2>/dev/null || echo b8492)"
fail=0

if grep -rE '0\.0\.0\.0' "$ROOT/server" "$ROOT/client" 2>/dev/null | grep -v check-security; then
  echo "FAIL: non-localhost rpc-server bind"; fail=1
fi

OFFICIAL="$(grep -v '^#' "$ROOT/config/official-worker.url" 2>/dev/null | grep -v '^$' | head -1 || true)"
[[ "$OFFICIAL" =~ ^https:// ]] \
  || { echo "FAIL: config/official-worker.url must contain official https Worker URL"; fail=1; }

if grep -rE '0\.0\.0\.0' "$ROOT/config" 2>/dev/null | grep -v check-security; then
  echo "FAIL: 0.0.0.0 in config/examples"; fail=1
fi

if [[ -f "$ROOT/server/volunteer.c" ]]; then
  grep -q '127.0.0.1' "$ROOT/server/volunteer.c" || { echo "FAIL: volunteer.c localhost bind"; fail=1; }
  grep -qE 'skip[-_]tunnel|skip_tunnel' "$ROOT/server/volunteer.c" && { echo "FAIL: volunteer skip-tunnel"; fail=1; }
fi

if [[ -f "$ROOT/client/moontail.c" ]]; then
  grep -q 'worker_is_localhost' "$ROOT/client/moontail.c" || { echo "FAIL: moontail localhost guard"; fail=1; }
  grep -q 'refused: --skip-tunnel' "$ROOT/client/moontail.c" || { echo "FAIL: moontail skip-tunnel refuse"; fail=1; }
fi

if ! git -C "$ROOT/vendor/llama.cpp" rev-parse HEAD >/dev/null 2>&1; then
  echo "FAIL: llama.cpp submodule missing"; fail=1
else
  cd "$ROOT/vendor/llama.cpp"
  git merge-base --is-ancestor "$(git rev-list -1 "$MIN_TAG" 2>/dev/null || echo NONE)" HEAD 2>/dev/null \
    || git describe --tags --always | grep -qE 'b849[2-9]|b85[0-9]{2}|b9' \
    || echo "WARN: verify llama.cpp >= $MIN_TAG ($(git rev-parse --short HEAD))"
fi

grep -q 'security control, not a version preference' "$ROOT/docs/SECURITY.md" \
  || { echo "FAIL: SECURITY.md version-pin policy"; fail=1; }

[[ $fail -eq 0 ]] && echo "OK: security checks passed" || exit 1
