#!/usr/bin/env bash
# Enforce thin-plan LOC budget (client <550, server <2000, tools+scripts <350, total <2800)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

count_dir() {
  local dir="$1"
  if [[ ! -d "$dir" ]]; then echo 0; return; fi
  find "$dir" -type f \( -name '*.c' -o -name '*.h' -o -name '*.ts' -o -name '*.py' -o -name '*.sh' \) \
    ! -path '*/vendor/*' -exec cat {} + 2>/dev/null | wc -l | tr -d ' '
}

client=$(count_dir "$ROOT/client")
server=$(count_dir "$ROOT/server")
tools=$(count_dir "$ROOT/tools")
scripts=$(count_dir "$ROOT/scripts")
docs_sec=0
[[ -f "$ROOT/docs/SECURITY.md" ]] && docs_sec=$(wc -l < "$ROOT/docs/SECURITY.md" | tr -d ' ')
total=$((client + server + tools + scripts + docs_sec))

echo "LOC: client=$client server=$server tools=$tools scripts=$scripts docs/SECURITY=$docs_sec total=$total"

fail=0
[[ $client -gt 550 ]] && echo "FAIL: client $client > 550" && fail=1
[[ $server -gt 2000 ]] && echo "FAIL: server $server > 2000" && fail=1
[[ $((tools + scripts + docs_sec)) -gt 350 ]] && echo "FAIL: tools+scripts+SECURITY $((tools+scripts+docs_sec)) > 350" && fail=1
[[ $total -gt 2800 ]] && echo "FAIL: total $total > 2800" && fail=1

if [[ $fail -eq 0 ]]; then
  echo "OK: within LOC budget"
else
  exit 1
fi
