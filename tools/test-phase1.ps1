param([string]$Compiler = 'C:\Program Files\LLVM\bin\clang.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force build/phase1 | Out-Null
    . ./tools/test-process.ps1
    $flags = @('-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-D_CRT_SECURE_NO_WARNINGS','-O2','-Isrc')
    Run-Checked $Compiler ($flags + @('tests/cpu_context_test.c','src/6502cpu.c','src/6502memory.c','-o','build/phase1/context.exe'))
    Run-Checked './build/phase1/context.exe' @()
    Run-Checked $Compiler ($flags + @('tests/context_trace.c','src/6502cpu.c','src/6502memory.c','-o','build/phase1/trace.exe'))
    $trace = (Run-Checked './build/phase1/trace.exe' @('programs/conway_life_6502.bin')) -split '\r?\n'

    $trace | Set-Content build/phase1/context-trace.txt
    if (Compare-Object (Get-Content tests/baseline-trace.txt) $trace) { throw 'Baseline trace differs' }
    Write-Output "All $($trace.Count) baseline checkpoints match."
    Run-Checked $Compiler ($flags + @('src/6502test.c','src/6502cpu.c','src/6502memory.c','src/6502framebuffer.c','src/6502machine.c','src/6502debugger.c','tools/headless_io.c','-o','build/phase1/headless.exe'))
    Run-Checked './build/phase1/headless.exe' @('--self-test')
    Run-Checked './build/phase1/headless.exe' @('--ticks','1000000','programs/conway_life_6502.bin')
    Run-Checked $Compiler ($flags + @('-Iinclude/curses','-fsyntax-only','src/6502test.c','src/6502cpu.c','src/6502memory.c','src/6502framebuffer.c','src/6502machine.c','src/6502debugger.c','src/6502io.c'))
} finally { Pop-Location }



