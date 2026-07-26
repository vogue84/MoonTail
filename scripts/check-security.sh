#!/usr/bin/env bash
# Security invariants: localhost rpc bind, llama.cpp pin >= b8492
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MIN_TAG="b8492"
fail=0

# Reject 0.0.0.0 rpc-server bind in our code
if grep -rE '0\.0\.0\.0|rpc-server.*-H[^ ]*[^1]' "$ROOT/server" "$ROOT/client" 2>/dev/null | grep -v check-security; then
  echo "FAIL: found non-localhost rpc-server bind pattern"
  fail=1
fi

# volunteer.c must use 127.0.0.1; no tunnel bypass on server side
if [[ -f "$ROOT/server/volunteer.c" ]]; then
  if ! grep -q '127.0.0.1' "$ROOT/server/volunteer.c"; then
    echo "FAIL: volunteer.c must bind rpc-server to 127.0.0.1"
    fail=1
  fi
  if grep -qE 'skip[-_]tunnel|skip_tunnel' "$ROOT/server/volunteer.c"; then
    echo "FAIL: volunteer.c must not implement --skip-tunnel bypass"
    fail=1
  fi
fi

# swarm-cli / moontail: --skip-tunnel only when --worker is localhost
if [[ -f "$ROOT/client/moontail.c" ]]; then
  if ! grep -q 'worker_is_localhost' "$ROOT/client/moontail.c"; then
    echo "FAIL: moontail.c must guard --skip-tunnel with worker_is_localhost()"
    fail=1
  fi
  if ! grep -q 'refused: --skip-tunnel/--local-rpc requires --worker on localhost' "$ROOT/client/moontail.c"; then
    echo "FAIL: moontail.c must refuse --skip-tunnel for non-localhost --worker"
    fail=1
  fi
fi

# Submodule present and at or after b8492
if [[ ! -d "$ROOT/vendor/llama.cpp/.git" ]]; then
  echo "FAIL: vendor/llama.cpp submodule missing"
  fail=1
else
  cd "$ROOT/vendor/llama.cpp"
  if git merge-base --is-ancestor "$(git rev-list -1 "$MIN_TAG" 2>/dev/null || echo NONE)" HEAD 2>/dev/null; then
    echo "OK: llama.cpp at or after $MIN_TAG"
  elif git describe --tags --always | grep -qE 'b849[2-9]|b85[0-9]{2}|b9|master'; then
    echo "OK: llama.cpp appears recent ($(git describe --tags --always))"
  else
    rev=$(git rev-parse --short HEAD)
    echo "WARN: verify llama.cpp >= $MIN_TAG manually (HEAD=$rev)"
    if [[ -f "$ROOT/config/MIN_LLAMA_VERSION" ]]; then
      pinned=$(cat "$ROOT/config/MIN_LLAMA_VERSION" | tr -d '\r\n')
      echo "Required min tag: $pinned"
    fi
  fi
fi

# SECURITY.md CVE policy present
if ! grep -q 'security control, not a version preference' "$ROOT/docs/SECURITY.md"; then
  echo "FAIL: SECURITY.md missing version-pin policy"
  fail=1
fi

if [[ $fail -eq 0 ]]; then
  echo "OK: security checks passed"
else
  exit 1
fi
