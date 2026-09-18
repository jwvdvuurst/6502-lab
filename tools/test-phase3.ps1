param([string]$Compiler = 'C:\Program Files\LLVM\bin\clang.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force build/phase3 | Out-Null
    . ./tools/test-process.ps1
    $flags = @('-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-D_CRT_SECURE_NO_WARNINGS','-Isrc')
    $core = @('src/6502cpu.c','src/6502memory.c','src/6502framebuffer.c','src/6502machine.c','src/6502debugger.c')
    foreach ($optimization in @('-O0','-O2')) {
        foreach ($test in @('machine','debugger')) {
            Run-Checked $Compiler ($flags + @($optimization,"tests/${test}_test.c") + $core + @('-o',"build/phase3/$test.exe"))
            Run-Checked "./build/phase3/$test.exe" @()
        }
    }
    Run-Checked $Compiler ($flags + @('-O2','-DTRACE_MACHINE','tests/context_trace.c') + $core + @('-o','build/phase3/undo-trace.exe'))
    $trace = (Run-Checked './build/phase3/undo-trace.exe' @('programs/conway_life_6502.bin')) -split '\r?\n'
    $trace | Set-Content build/phase3/undo-trace.txt
    if (Compare-Object (Get-Content tests/baseline-trace.txt) $trace) { throw 'Machine trace differs' }
    Write-Output 'All 1001 Conway checkpoints match through machine_step; 1000 batches of undo/re-execution passed.'
    Run-Checked $Compiler ($flags + @('-O2','src/6502test.c') + $core + @('tools/headless_io.c','-o','build/phase3/6502headless.exe'))
    [IO.File]::WriteAllBytes((Join-Path (Get-Location) 'build/phase3/debugger.bin'), [byte[]](0xA9,0x2A,0x85,0x10,0xE8,0x4C,0,8))
    $commands = "step 2`nmem 10 1`nback 2`nmem 10 1`nbreak 0804`nrun 10`nhistory`nquit`n"
    $session = Run-Checked './build/phase3/6502headless.exe' @('--debugger','--history','8','build/phase3/debugger.bin','0x0800') $commands
    if ($session -notmatch '0010: 2A' -or $session -notmatch '0010: EA' -or $session -notmatch 'Breakpoint hit at 0804' -or $session -notmatch 'capacity=8') { throw 'CLI debugger smoke failed' }
    $session | Set-Content build/phase3/debugger-session.txt
    foreach ($arguments in @(
        ,@('--debugger'),
        @('--debugger','--history','0','build/phase3/debugger.bin'),
        @('--debugger','--history','65537','build/phase3/debugger.bin'),
        @('--history','8','build/phase3/debugger.bin'),
        @('--debugger','--ticks','1','build/phase3/debugger.bin')
    )) { Run-Checked './build/phase3/6502headless.exe' $arguments '' 1 }
    Write-Output 'Actual debugger CLI session and invalid option checks passed.'
    & ./tools/test-phase2.ps1 -Compiler $Compiler
} finally { Pop-Location }

