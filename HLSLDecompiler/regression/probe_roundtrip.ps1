param(
    [string]$Decompiler = "$PSScriptRoot\..\cmd_Decompiler\builds\x64\Release\cmd_Decompiler.exe",
    [string]$Fxc = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\fxc.exe'
)
$ErrorActionPreference = 'Stop'
$work = Join-Path $PSScriptRoot ('out-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot\fixtures\probe_raw.asm" -Destination "$work\probe.asm"
function Run-Checked([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed: $LASTEXITCODE" }
}
Run-Checked $Decompiler @('-a', "$work\probe.asm")
Run-Checked $Decompiler @('-D', "$work\probe.shdr")
Run-Checked $Fxc @('/nologo', '/T', 'ps_5_0', '/E', 'main', '/O3', '/Fo', "$work\probe.bin", '/Fc', "$work\probe.roundtrip.asm", "$work\probe.hlsl")
$hlsl = Get-Content -LiteralPath "$work\probe.hlsl" -Raw
$asm = Get-Content -LiteralPath "$work\probe.roundtrip.asm" -Raw
if ($hlsl -match 'asint\([^)]*e-4[0-9]') { throw 'Integer immediate emitted through a denormal float' }
if ($asm -notmatch 'imin[^\r\n]*l\(31, 15, 23, 0\)') { throw 'Tile clamp lost 31/15/23' }
if ($asm -notmatch 'dcl_resource_structured t8, 4') { throw 'Missing t8 binding' }
if ($asm -notmatch 'ld_structured[^\r\n]*r\d+\.[xyzw]{2},[^\r\n]*l\(0\), t7\.') { throw 'Missing two-word mask load' }
if ($asm -notmatch '\butof\b' -or $asm -match '\bitof\b') { throw 'Unsigned conversion changed to signed' }
$depth = 0
$bits = 0
$lookups = 0
$probeRegister = $null
foreach ($line in ($asm -split '\r?\n')) {
    $line = $line.Trim()
    if ($line -match '^loop\b') { ++$depth }
    if ($line -match '^endloop\b') { --$depth }
    if ($line -match '^firstbit_lo\b') {
        if ($depth -ne 1) { throw 'Probe selection escaped its loop' }
        ++$bits
    }
    if ($line -match '^ld_structured.*?\) (r\d+\.[xyzw]), (r\d+\.[xyzw]), l\(0\), t8\.') {
        if ($depth -ne 1) { throw 'Probe lookup escaped its loop' }
        $probeRegister = $Matches[1]
        ++$lookups
    } elseif ($probeRegister -and $line -match '^ld_structured.*t6\.') {
        if ($line -notmatch (', ' + [regex]::Escape($probeRegister) + ', l\(96\), t6\.')) {
            throw 't6 metadata does not consume the t8 result'
        }
        $probeRegister = $null
    }
}
if ($bits -ne 2 -or $lookups -ne 2 -or $depth -ne 0 -or $probeRegister) {
    throw "Incomplete probe loops: firstbit=$bits t8=$lookups depth=$depth"
}
Write-Host "PASS RAW probe roundtrip (optimized FXC); artifacts: $work"
