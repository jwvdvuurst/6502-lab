# Small debugger quick start

From the 6502-lab repository in PowerShell 7, build and verify:

```powershell
./tools/test-phase3.ps1
./build/phase3/6502headless.exe --debugger programs/conway_life_6502.bin
```

Alternatively `make headless` builds the main-directory executable; launch it
with `./6502headless.exe --debugger programs/conway_life_6502.bin`.
Do rebuild: a pre-existing main-directory executable may be from an earlier phase.

The default program load address is $D000. Supply an explicit address as the final
argument when needed. PRG files use their embedded address unless overridden.
History holds 4,096 instructions/edits by default. Set it at startup with
`--debugger --history 16384 programs/conway_life_6502.bin` (maximum 65,536 entries).
Debugger mode requires a program and cannot be combined with --ticks, --dump-screen
or --self-test. It starts paused, before the first guest instruction.

Try these commands at the `dbg>` prompt:

```text
regs
step 10
mem c000 64
back 10
history
run 1000
screen
quit
```

Addresses and byte values use hexadecimal (with or without `0x`); counts are decimal.

| Command | Effect |
| --- | --- |
| `help` | List commands and limits |
| `step [n]` | Execute n instructions; default 1 |
| `back [n]` | Undo n instructions or edits; default 1 |
| `run [n]` | Execute up to n instructions; default 10,000 |
| `break d000` | Set the single execution breakpoint |
| `break off` | Clear the breakpoint |
| `regs` | Registers, cycle counter, stack depth and next three raw bytes |
| `mem c000 64` | Inspect 64 bytes; maximum 256, address wraps at $FFFF |
| `poke c000 41` | Write byte $41, recording an undoable edit |
| `pc d000` | Change PC, recording an undoable edit |
| `history` | Current, oldest and newest retained positions and capacity |
| `screen` | Read the restored physical framebuffer as 40x25 text |
| `quit` | Exit; EOF also exits |

Run stops BEFORE an instruction at the breakpoint. If paused on it, use `step` to
execute that instruction before continuing with `run`, or clear the breakpoint.
Step ignores the breakpoint. Run/step/back counts are bounded at 1,000,000.

The oldest history is evicted when capacity is reached. Back reports when it
cannot go farther. After undo, a successful step/run or edit replaces the future;
stepping forward executes guest code again. It does not redo former debugger edits.
Edits count as history entries alongside instructions. Failed operations roll back
without changing history. Changing PC does not reset registers or the stack.

Console output and breakpoint settings are not undone. `screen` always reads the
current physical framebuffer, so it reflects undo without relying on a UI cache.
The three next bytes are a raw preview; mnemonic/source disassembly is later work.
