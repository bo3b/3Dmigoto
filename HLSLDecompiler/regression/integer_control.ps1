param(
    [string]$Decompiler = "$PSScriptRoot\..\cmd_Decompiler\builds\x64\Release\cmd_Decompiler.exe",
    [string]$Fxc = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\fxc.exe'
)
$ErrorActionPreference = 'Stop'
$work = Join-Path $PSScriptRoot ('out-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot\fixtures\integer_control.asm" -Destination "$work\shader.asm"
& $Decompiler -a "$work\shader.asm"
if ($LASTEXITCODE) { throw 'Assembly failed' }
& $Decompiler -D "$work\shader.shdr"
if ($LASTEXITCODE) { throw 'Decompilation failed' }
& $Fxc /nologo /O3 /T ps_5_0 /E main /Fo "$work\shader.bin" "$work\shader.hlsl"
if ($LASTEXITCODE) { throw 'Pixel shader compilation failed' }
& cl /nologo /EHsc /std:c++14 "$PSScriptRoot\integer_control_gpu.cpp" "/Fo:$work\gpu.obj" "/Fe:$work\gpu.exe" /link d3d11.lib d3dcompiler.lib
if ($LASTEXITCODE) { throw 'GPU harness build failed (run in an MSVC developer shell)' }
& "$work\gpu.exe" "$work\shader.hlsl"
if ($LASTEXITCODE) { throw 'GPU result mismatch' }
