<#
.SYNOPSIS
    Build the portable zip of Gorilla TPFanControl from the same files the
    installer ships. Run from anywhere; writes installer\output\*-portable.zip.

.DESCRIPTION
    Contents: TPFanControl.exe (Release build), LpcACPIEC.bin, the default
    TPFanControl.ini, the licences, README-PORTABLE.txt, the two launchers,
    and PawnIO\PawnIO_setup.exe (checked against the SHA-256 in winget's
    manifest before it is packed; the build refuses on a mismatch).
#>
$ErrorActionPreference = 'Stop'
$root   = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$inst   = Join-Path $root 'installer'
$ver    = ([regex]::Match((Get-Content (Join-Path $inst 'GorillaTPFanControl.iss') -Raw), '#define AppVersion\s+"([^"]+)"')).Groups[1].Value
$pawn   = Join-Path $inst 'thirdparty\PawnIO\PawnIO_setup.exe'
$pawnSha = '1F519A22E47187F70A1379A48CA604981C4FCF694F4E65B734AAA74A9FBA3032'   # winget manifest, PawnIO 2.2.0

if ((Get-FileHash $pawn -Algorithm SHA256).Hash -ne $pawnSha) { throw "PawnIO_setup.exe does not match the expected SHA-256 - refusing to pack it" }

$name  = "Gorilla-TPFanControl-$ver-portable"
$stage = Join-Path $env:TEMP $name
if (Test-Path $stage) { Get-ChildItem $stage -Recurse -File | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }; Get-ChildItem $stage -Recurse -Directory | Sort-Object FullName -Descending | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }; Remove-Item -LiteralPath $stage -Force }
New-Item -ItemType Directory -Path "$stage\licences", "$stage\PawnIO" -Force | Out-Null

Copy-Item (Join-Path $root 'fancontrol\Release\TPFanControl.exe') $stage
Copy-Item (Join-Path $root 'fancontrol\pawnio\LpcACPIEC.bin')    $stage
Copy-Item (Join-Path $root 'fancontrol\TPFanControl.ini')        $stage
Copy-Item (Join-Path $inst 'portable\*')                          $stage
Copy-Item $pawn                                                   "$stage\PawnIO"
Copy-Item (Join-Path $root 'fancontrol\pawnio\COPYING')          "$stage\licences\LpcACPIEC-LGPL-2.1.txt"
Copy-Item (Join-Path $root 'LICENSE')                            "$stage\licences\TPFanControl-Unlicense.txt"
Copy-Item (Join-Path $inst 'THIRD-PARTY-NOTICES.txt')            "$stage\licences"
# batch files must be CRLF
Get-ChildItem $stage -Filter *.cmd | ForEach-Object { [IO.File]::WriteAllText($_.FullName, ([IO.File]::ReadAllText($_.FullName) -replace "`r?`n", "`r`n")) }

$zip = Join-Path $inst "output\$name.zip"
New-Item -ItemType Directory -Path (Split-Path $zip) -Force | Out-Null
if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Write-Host ("built {0}  {1:N0} bytes" -f $zip, (Get-Item $zip).Length)
Get-ChildItem $stage -Recurse -File | ForEach-Object { Write-Host ("  {0}" -f $_.FullName.Substring($stage.Length + 1)) }
