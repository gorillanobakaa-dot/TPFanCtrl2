<#
.SYNOPSIS
    Build Gorilla TPFanControl for Linux on Windows: tests, the x86-64 Linux
    binary, and the .deb and .rpm packages. Writes linux\dist\.

.DESCRIPTION
    Needs: Rust (rustup target add x86_64-unknown-linux-gnu), zig, and
    cargo install cargo-zigbuild cargo-deb cargo-generate-rpm.
    The binary is linked against glibc 2.28 (Debian 10, RHEL 8 and newer), so
    it runs on every maintained Debian, Ubuntu and Fedora release.
#>
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here
$target = 'x86_64-unknown-linux-gnu'

cargo test --release
if ($LASTEXITCODE) { throw 'tests failed' }

cargo zigbuild --release --target "$target.2.28"
if ($LASTEXITCODE) { throw 'Linux build failed' }

cargo deb --no-build --target $target --output dist
if ($LASTEXITCODE) { throw 'cargo deb failed' }

cargo generate-rpm --target $target --output dist
if ($LASTEXITCODE) { throw 'cargo generate-rpm failed' }

Copy-Item "target\$target\release\gorilla-fan" dist\gorilla-fan-linux-x86_64 -Force
Get-ChildItem dist | ForEach-Object { '{0}  {1,12:N0} B  {2}' -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash, $_.Length, $_.Name }
