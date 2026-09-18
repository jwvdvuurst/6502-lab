# Phase 2 — memory bus and physical blocks

Completed and verified: 13 September 2026.

## Architecture and ownership

`physical_memory_t` borrows caller-owned storage divided into 1 KiB blocks.
`address_space_t` maps 64 logical windows onto physical block IDs. Initialization
provides the original 64 KiB identity mapping. The CLI allocates 64 blocks;
tests configure additional blocks. Block IDs are 16-bit, with storage-size
validation limiting the model to 65,536 blocks. No large default allocation.

CPUs now borrow an address_space_t rather than a raw buffer or cached stack pointer.
All CPU reads/writes, instruction operands, reset vectors and stack operations
use the bus. Reset preserves the mapping and physical storage. Each byte of a
word is translated separately, including a 1 KiB edge and $FFFF/$0000 wraparound.
Zero-page pointer wrap remains an explicit CPU rule.

Host-only address_space_map validates a window/block before changing one mapping;
invalid requests leave it unchanged. Configuration occurs between instructions.
Unmapped data remains in its physical block. No guest MAP opcode, bank loader
format, scheduling or guest fault policy has been introduced.

The binary/PRG and built-in loaders receive an address space and use bus writes.
The old global memory buffer/access functions have been removed. The memory
self-test now uses private storage; the CLI explicitly fills guest memory with
$EA to preserve its previous startup image.

## Framebuffer and writes

The frontend binds a framebuffer_t to physical block 48 (the original $C000 block).
Display reads and all clear/text/scroll writes use physical memory accessors.
Changing the CPU's window 48 does not redirect the display. Headless dumps use
the same framebuffer binding. The reusable framebuffer helpers are tested without
curses. Text helpers reject coordinates or strings beyond the visible 40x25 grid;
this also removes old length truncation and out-of-range write possibilities.
Clear still fills the entire 1 KiB block, and scrolling >=24 lines retains the
original clear behavior.

memory_write8 is the single production backing-store write point. Its optional,
synchronous observer receives physical block, offset, old byte and new byte before
every write, including unchanged values. It must not mutate/reenter the bus.
This is a tested observation hook for later history work, not a journal or undo.

## API contract

Keep storage, physical memory, address spaces and framebuffer bindings alive while
in use. Initialize CPUs sequentially, as in Phase 1. Use configuration APIs rather
than mutating mapping internals. Failed configuration leaves valid state intact.
Byte access requires valid initialized bindings and in-range physical addresses;
debug assertions detect programming errors. Guest-visible bus/device faults and
transaction rollback remain later work. No host concurrency guarantee.

## Validation

Run `./tools/test-phase2.ps1` in PowerShell 7. Each compiler/test process has a
30-second wall-clock timeout, captures its output, and fails on nonzero exit.
The complete staged run took approximately nine seconds on this machine.

Passed with Clang C11, -Wall -Wextra -Wpedantic -Werror:

- Phase 2 tests at -O0 and -O2.
- Identity mapping over all 65,536 addresses and all 64 window edges.
- Nonadjacent physical blocks, word wraparound, retained/aliased storage and
  rejected mappings/storage sizes without mutation.
- Mapped reset vectors, instruction/operand fetch across windows and address wrap,
  zero-page pointer wrap, nested JSR/RTS, stack saturation and pop order.
- CPU stores/read-modify-writes and ordered physical write observations.
- Fixed framebuffer binding after CPU remapping; clear, text, long strings,
  bounds checks and scrolling through the same physical write path.
- Actual CLI binary/PRG loader writes across nonadjacent blocks, mapped reset-vector
  writes, stop at $FFFF, and malformed PRG/source-file rejection.
- Phase 1 CPU-isolation tests and existing self-tests.
- All 1,001 archived Conway checkpoints match for 1,000,000 instructions, including
  registers, cycles, stack depth and full-memory hashes.
- Final checkpoint: `1000000 00 00 0C FD 22 D0D9 85 2981297 2 6A539D18F476C9F2`.
- Interactive frontend and all production C files pass syntax checking.

The compiler script suppresses the existing Windows fopen deprecation warning.
The interactive executable remains unlinked/unrun with this MSVC-targeting Clang
and bundled MinGW curses archive. Full instruction conformance and cycle accuracy
are not claimed. Existing stack handlers ignore push/pop failure return values;
tests preserve that behavior rather than changing instruction semantics here.

## Integration and next phase

Work was developed in an isolated copy and validated before promotion. The
pre-Phase-2 source, tests, scripts and build instructions are retained in
phase2-baseline-source.zip. Earlier phase reports are historical records.
No Phase 2 acceptance criteria remain. Phase 3 is instruction transactions and
undo for one CPU, including register state and physical memory write journaling.
