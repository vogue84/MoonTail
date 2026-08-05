#!/usr/bin/env bash
# LOC budget: client<750 server<2000 tools+scripts<400 total<3000
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

count_dir() {
  local dir="$1"
  [[ -d "$dir" ]] || { echo 0; return; }
  find "$dir" -type f \( -name '*.c' -o -name '*.h' -o -name '*.ts' -o -name '*.py' -o -name '*.sh' \) \
    ! -path '*/vendor/*' -exec cat {} + 2>/dev/null | wc -l | tr -d ' '
}

client=$(count_dir "$ROOT/client")
server=$(count_dir "$ROOT/server")
tools=$(count_dir "$ROOT/tools")
scripts=$(count_dir "$ROOT/scripts")
docs_sec=0
[[ -f "$ROOT/docs/SECURITY.md" ]] && docs_sec=$(wc -l < "$ROOT/docs/SECURITY.md" | tr -d ' ')
aux=$((tools + scripts))
total=$((client + server + aux + docs_sec))

echo "LOC: client=$client server=$server tools=$tools scripts=$scripts docs/SECURITY=$docs_sec total=$total"

fail=0
[[ $client -gt 750 ]] && echo "FAIL: client $client > 750" && fail=1
[[ $server -gt 2000 ]] && echo "FAIL: server $server > 2000" && fail=1
[[ $aux -gt 400 ]] && echo "FAIL: tools+scripts $aux > 400" && fail=1
[[ $total -gt 3000 ]] && echo "FAIL: total $total > 3000" && fail=1

[[ $fail -eq 0 ]] && echo "OK: within LOC budget" || exit 1
