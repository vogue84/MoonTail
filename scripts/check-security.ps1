# Security invariants (PowerShell parity with check-security.sh)
$Root = Split-Path -Parent $PSScriptRoot
$fail = $false

if (Select-String -Path "$Root/server/volunteer.c" -Pattern 'skip[-_]tunnel|skip_tunnel' -Quiet) {
    Write-Host "FAIL: volunteer.c must not implement --skip-tunnel bypass"
    $fail = $true
}
if (-not (Select-String -Path "$Root/client/moontail.c" -Pattern 'worker_is_localhost' -Quiet)) {
    Write-Host "FAIL: moontail.c must guard --skip-tunnel with worker_is_localhost()"
    $fail = $true
}
if (-not (Select-String -Path "$Root/client/moontail.c" -Pattern 'refused: --skip-tunnel' -Quiet)) {
    Write-Host "FAIL: swarm-cli.c must refuse --skip-tunnel for non-localhost --worker"
    $fail = $true
}
if (-not (Select-String -Path "$Root/docs/SECURITY.md" -Pattern 'security control, not a version preference' -Quiet)) {
    Write-Host "FAIL: SECURITY.md missing version-pin policy"
    $fail = $true
}
if (-not $fail) { Write-Host "OK: security checks passed (PS)" } else { exit 1 }
