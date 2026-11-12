<#
NeonPine on Windows: build, flash and control the tree from PowerShell. See README.md "Windows".

  .\np.ps1 build                 build bootloader + firmware + installer (MSYS2 make + riscv64-unknown-elf-gcc)
  .\np.ps1 flash [-Wait 60]      build, then flash the firmware over USB (verified; the store is kept)
  .\np.ps1 flash-mc              build, then flash with minichlink instead (make flash)
  .\np.ps1 make <target>         any Makefile target (first-flash, flash-swd, clean, ...)
  .\np.ps1 app                   run the NeonPine Companion app from source
  .\np.ps1 status | watch | show garland | style retro | bright 128 | ...   npctl.py (tools/npctl.py)
#>
param(
    [Parameter(Position = 0)] [string] $Cmd = "status",
    [double] $Wait = 8,
    [string] $Image = "",
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)] [string[]] $Rest = @()
)
$ErrorActionPreference = "Stop"
$fw = $PSScriptRoot
$root = Split-Path $fw -Parent
$msys = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }

function Invoke-Make([string[]] $targets) {
    if (-not (Test-Path "$msys\mingw64\bin\riscv64-unknown-elf-gcc.exe")) {
        throw "No RISC-V toolchain in $msys. Install it: $msys\usr\bin\pacman -S mingw-w64-x86_64-riscv64-unknown-elf-toolchain"
    }
    $old = $env:PATH
    try {
        $env:PATH = "$msys\mingw64\bin;$msys\usr\bin;$old"
        # NULLDEV: ch32fun picks "nul" on Windows_NT, but make runs MSYS sh, where 2>nul creates a file named nul.
        & "$msys\usr\bin\make.exe" -C $fw NULLDEV=/dev/null @targets
        if ($LASTEXITCODE) { throw "make $targets failed ($LASTEXITCODE)" }
    } finally { $env:PATH = $old }
}

function Get-Python {
    $venv = Join-Path $root "companion\.venv\Scripts\python.exe"
    if (Test-Path $venv) { return $venv }
    return "py"
}

$py = Get-Python
switch ($Cmd) {
    "build"    { Invoke-Make @("build") }
    "flash"    {
        Invoke-Make @("build")
        $img = if ($Image) { $Image } else { Join-Path $fw "neonpine\neonpine.bin" }
        & $py (Join-Path $fw "tools\npflash.py") $img --wait $Wait
        exit $LASTEXITCODE
    }
    "flash-mc" { Invoke-Make @("flash") }
    "make"     { Invoke-Make $Rest }
    "app"      { & $py (Join-Path $root "companion\neonpine-companion.py") @Rest; exit $LASTEXITCODE }
    "status"   { & $py (Join-Path $fw "tools\npctl.py"); exit $LASTEXITCODE }
    default    { & $py (Join-Path $fw "tools\npctl.py") $Cmd @Rest; exit $LASTEXITCODE }
}
