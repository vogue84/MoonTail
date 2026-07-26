# LOC budget check (PowerShell)
$Root = Split-Path -Parent $PSScriptRoot
function Count-Dir($dir) {
    if (-not (Test-Path $dir)) { return 0 }
    $files = Get-ChildItem -Path $dir -Recurse -Include *.c,*.h,*.ts,*.py,*.sh -File -ErrorAction SilentlyContinue
    ($files | Get-Content | Measure-Object -Line).Lines
}
$client = Count-Dir "$Root/client"
$server = Count-Dir "$Root/server"
$tools = Count-Dir "$Root/tools"
$scripts = Count-Dir "$Root/scripts"
$docsSec = if (Test-Path "$Root/docs/SECURITY.md") { (Get-Content "$Root/docs/SECURITY.md" | Measure-Object -Line).Lines } else { 0 }
$total = $client + $server + $tools + $scripts + $docsSec
Write-Host "LOC: client=$client server=$server tools=$tools scripts=$scripts docs/SECURITY=$docsSec total=$total"
$fail = $false
if ($client -gt 550) { Write-Host "FAIL: client $client > 550"; $fail = $true }
if ($server -gt 2000) { Write-Host "FAIL: server $server > 2000"; $fail = $true }
if (($tools + $scripts + $docsSec) -gt 350) { Write-Host "FAIL: tools+scripts+SECURITY > 350"; $fail = $true }
if ($total -gt 2800) { Write-Host "FAIL: total $total > 2800"; $fail = $true }
if (-not $fail) { Write-Host "OK: within LOC budget" } else { exit 1 }
