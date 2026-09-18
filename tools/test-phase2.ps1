param([string]$Compiler = 'C:\Program Files\LLVM\bin\clang.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force build/phase2 | Out-Null
    . ./tools/test-process.ps1
    $flags = @('-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-D_CRT_SECURE_NO_WARNINGS','-Isrc')
    $core = @('src/6502cpu.c','src/6502memory.c','src/6502framebuffer.c','src/6502machine.c','src/6502debugger.c')
    foreach ($optimization in @('-O0','-O2')) {
        Run-Checked $Compiler ($flags + @($optimization,'tests/memory_bus_test.c') + $core + @('-o','build/phase2/bus.exe'))
        Run-Checked './build/phase2/bus.exe' @()
        Run-Checked $Compiler ($flags + @($optimization,'tests/loader_bus_test.c') + $core + @('tools/headless_io.c','-o','build/phase2/loader.exe'))
        Run-Checked './build/phase2/loader.exe' @()
    }
    & ./tools/test-phase1.ps1 -Compiler $Compiler

} finally { Pop-Location }


