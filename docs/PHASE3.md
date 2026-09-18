# Phase 3 — single-CPU undo and small debugger

Completed and verified: 13 September 2026.

## What changed

`6502machine.c/.h` adds a single-CPU transaction owner with a bounded history ring.
It borrows the initialized CPU, address space and physical storage. History is
allocated once at creation; no allocation occurs while stepping or undoing.
Each entry saves the CPU execution state, all 64 mapping entries, and physical
block/offset/old-byte records in write order. Undo applies writes in reverse order
and then restores the mapping and CPU, including PC, flags, cycles, stack depth,
current opcode and pending status request. Repeated writes to the same byte and
writes across mapping changes restore correctly.

`machine_step` executes one existing CPU instruction as a transaction.
`machine_step_back` restores the previous instruction or debugger edit. Successful
new execution/edits after undo discard the old future. Forward execution after
undo re-executes the guest instruction; this is not a replay of old debugger edits.
A failed action leaves the cursor and retained future unchanged.

History defaults to 4,096 entries, configurable from 1 to 65,536. When full, the
oldest entry is evicted. Current, oldest and newest positions are exposed. These
positions count committed instructions AND edits and may be reused after branching.
The present write limit is 16 byte writes per transaction; current instructions
fit within it. Oversized future operations need a deliberate capacity change.

The memory API now returns success from writes and has a pre-write guard. The
machine guard journals accepted writes and vetoes unrecordable ones BEFORE they
modify memory. If an action fails or exceeds its write budget, all accepted writes,
mappings and CPU state roll back. This remains true when an instruction/action
ignores a failed write result. Tests inject failures to validate this contract;
legacy instruction fault/stack behavior itself was not changed.

While a machine is attached, out-of-transaction physical writes are rejected.
Undo suppresses forward write notifications. Existing read-only observers remain
attached and resume afterward. Framebuffer helpers propagate write rejection.
Destroy the machine before releasing or rebinding borrowed objects. CPU mutation,
reset or host mapping changes must occur through a transaction while attached.
The API does not protect against callers directly modifying public structs.

## Debugger

`6502debugger.c/.h` implements a stream-based text console available through
`--debugger` in the headless build. It supports step/back/run, one execution
breakpoint, register and memory inspection, a physical framebuffer dump, undoable
memory edits and PC edits. No curses dependency is needed. See DEBUGGER.md.

Run is bounded (10,000 instructions by default, maximum 1,000,000). A breakpoint
stops run before executing that PC; step intentionally ignores it. EOF closes the
console. Invalid commands and overlong lines do not execute guest instructions.
Registers include a three-byte preview, explicitly not a disassembler.

The debugger attaches history after loading and initialization. Ordinary --ticks
and curses execution retain their prior direct execution path and do not record
history. Debugger screen output is read afresh from restored physical memory.
Breakpoint settings and console output are frontend state, not guest history.

## Validation

Run `./tools/test-phase3.ps1` in PowerShell 7. Each compiler/test has the existing
30-second process timeout; scripted console input is closed explicitly.

Passed with Clang C11, -Wall -Wextra -Wpedantic -Werror:

- Machine and debugger tests at -O0 and -O2.
- N forward / N backward returns all guest bytes, mappings and CPU fields to their
  initial values; re-executing N steps reproduces the saved final state.
- Nested subroutines and mapped stack writes; repeated physical writes; mapping
  changes inside transactions; observer suppression during undo.
- Injected action failure and write overflow restore state atomically and preserve
  prior history/future. Recursive transactions are rejected.
- Undoable edits, branching, ring wraparound/eviction, capacity one and oldest-state
  reporting; rejection of out-of-transaction memory/frontend writes.
- Scripted debugger sessions: breakpoint behavior, step/back, registers, memory,
  screen, undoable poke/PC edits, invalid numbers, extra arguments, long input,
  EOF and a final command without a newline.
- Actual --debugger CLI launch, configured capacity, memory restoration, breakpoint
  stop and rejection of invalid/conflicting options.
- All 1,001 archived Conway checkpoints match through machine_step for 1,000,000
  instructions. At each 1,000-instruction checkpoint, undo/re-execute 1,000 steps
  and compare the complete memory image and CPU execution state again. All 1,000
  batches pass, exercising bounded history and re-execution across the full run.
- All earlier Phase 1/2 regression tests, original self-tests and the direct Conway
  trace still pass. Interactive C frontend syntax checking passes.

Only the existing Windows fopen deprecation warning is suppressed by the scripts.
The final baseline checkpoint remains:
`1000000 00 00 0C FD 22 D0D9 85 2981297 2 6A539D18F476C9F2`.

## Scope and remaining work

Phase 3 acceptance criteria and the small debugger extension are complete.
The Phase 2 source, tests, scripts and build instructions are preserved in
phase3-baseline-source.zip before integration. The original emulator is untouched.

History is in-memory, single-CPU and instruction-level. There is no external-input
replay, persistent save, guest MAP opcode, thread scheduler, multi-CPU history,
watchpoint, source-level debugging or disassembler yet. Observer/host I/O effects
cannot be undone and must remain outside guest transactions. The curses executable
still has only syntax validation; the new text debugger is built and exercised.
Existing stack/instruction quirks and approximate cycle accounting are preserved.
Phase 4 remains explicit ISA/tooling support and guest banking.
