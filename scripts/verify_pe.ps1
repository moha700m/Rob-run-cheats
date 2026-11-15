# verify_pe.ps1 - Confirms a Windows executable exists, is a valid PE image and is x64.
param([Parameter(Mandatory = $true)][string]$Path)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Path)) { Write-Error "EXE not found: $Path"; exit 1 }
$bytes = [System.IO.File]::ReadAllBytes($Path)
if ($bytes.Length -lt 0x100 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) { Write-Error "Not an MZ executable"; exit 1 }

$peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
if ($peOffset -le 0 -or $peOffset + 0x5C -gt $bytes.Length) { Write-Error "Bad PE header offset"; exit 1 }
$sig = [BitConverter]::ToUInt32($bytes, $peOffset)
if ($sig -ne 0x00004550) { Write-Error "Missing PE signature"; exit 1 }

$machine   = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
$optMagic  = [BitConverter]::ToUInt16($bytes, $peOffset + 24)
$subsystem = [BitConverter]::ToUInt16($bytes, $peOffset + 24 + 68)
$hash      = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash

Write-Host ("File      : {0}" -f $Path)
Write-Host ("Size      : {0:N0} bytes" -f $bytes.Length)
Write-Host ("Machine   : 0x{0:X4} ({1})" -f $machine, $(if ($machine -eq 0x8664) { 'x64' } else { 'NOT x64' }))
Write-Host ("PE format : 0x{0:X3} ({1})" -f $optMagic, $(if ($optMagic -eq 0x20B) { 'PE32+' } else { 'PE32' }))
Write-Host ("Subsystem : {0} ({1})" -f $subsystem, $(if ($subsystem -eq 2) { 'Windows GUI' } else { 'other' }))
Write-Host ("SHA256    : {0}" -f $hash)

if ($machine -ne 0x8664 -or $optMagic -ne 0x20B) { Write-Error "Executable is not x64 (PE32+)"; exit 1 }
Write-Host "PASS: valid x64 PE32+ executable"
