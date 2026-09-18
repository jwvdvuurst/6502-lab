> Phase 3 and the small text debugger are complete and verified on 13 September 2026. See [PHASE3.md](PHASE3.md) and [DEBUGGER.md](DEBUGGER.md). Earlier phase statuses below are historical.

> Phase 2 completed and verified on 13 September 2026. See [PHASE2.md](PHASE2.md) for the current implementation; earlier phase status below is historical.

> Phase 1 completed and verified on 13 September 2026. See [PHASE1.md](PHASE1.md). Earlier status and code findings below are historical.

# 6502 Lab — discussion summary and continuation notes

Updated: 13 September 2026.
This captures the 6502 discussion from the Game of Life task. It is a summary,
not a relocated conversation or a completed implementation.

## Project and purpose

Create an experimental 6502-derived machine combining:
1. Banked memory in 1 KiB blocks.
2. Simple cooperative multithreading.
3. Multiple virtual processors with mostly private memory and a shared region.
4. A global time-travel debugger.
5. An assembler and disassembler supporting the custom instruction set.

Original project:
C:\Users\JWvdV\OneDrive\Documenten\workspace\6502emulator

Independent lab project:
C:\Users\JWvdV\Documents\workspace\6502-lab

The original must remain intact. The user's interest is an understandable,
reproducible experimental computer, not initially cycle-accurate hardware.

## Memory banking proposal

Keep the CPU's 16-bit logical address space: 64 KiB, divided into 64 windows
of 1 KiB each. Map each window to a physical backing block. Remapping changes
a reference, not a copy; previously written data remains in an unmapped block.

Conceptual syntax, not allocated opcodes:
- MAP slot, bank
- MAPR slot, bank, count (later)

A slot is the destination window; a bank is the physical source block.
Use 16-bit bank IDs as a proposal, with a configurable actual bank count.
A normal 6502 page remains 256 bytes; use "1 KiB block/window" for these units.

Fetch all instruction operands using the old mapping, validate, then change
mapping atomically at an instruction boundary. The next fetch uses the new
mapping. Word accesses must translate each byte separately, including window
boundaries and 16-bit address wraparound.

"Persistent" currently means retained while unmapped. Persistence across host
restarts needs a separate versioned save/load mechanism; this is not yet decided.

## Threads

Each thread needs its own registers (A/X/Y/PC/SP/P), mapping and execution state.
Give each thread its own first 1 KiB block ($0000-$03FF), covering both zero page
and the stack. Other code/data can remain shared.

Start with cooperative scheduling via YIELD and a wait operation, not timers.
Threads stay assigned to their virtual CPU. Initial thread creation can be
specified by the host's machine configuration; guest SPAWN comes later.

Candidate instructions include YIELD, HALT, WAIT and SIGNAL. Sticky pending-event
bits can prevent a notification immediately before WAIT from being lost.
These mnemonics and their exact semantics remain proposals.

## Multiple processors and communication

Each CPU has its own state and memory mapping. Private windows point to different
physical blocks; shared windows point to the same block. Shared blocks need not
appear at the same virtual address on each CPU.

Start with two processors and a shared mailbox at, for example, $8000-$83FF.
Use one producer/consumer in each direction, with payload and acknowledgement.
Shared memory alone does not provide synchronization.

An atomic test-and-set instruction may later support locks. SEI only affects
local interrupt handling and cannot stop another processor.

Initially execute virtual CPUs on one host thread, in deterministic round-robin
order, one complete instruction at a time. This models instruction-level
interleaving, not real bus cycles. A guest instruction that never yields can
starve threads on its own CPU but does not prevent another virtual CPU running.

## Time travel

Use a machine-wide history, not isolated per-CPU undo. If CPU B has consumed a
message from CPU A, undoing A's send must also rewind B's subsequent effects.

Record registers, scheduler state, mappings, events and memory writes.
Memory writes must identify physical block + offset + old value, not merely
the virtual address. Undo writes in reverse order.

Log external input for deterministic replay. Redraw the UI from restored state.
Bound the history buffer and expose its oldest available step. Debugger edits
after undo create a new history branch.

A last-writer/read trace is an additional feature for causal debugging; an undo
journal alone cannot explain which previous write supplied every read.

## Assembler and disassembler

The user correctly noted that adding opcodes also requires compatible tools.
The existing tools/asm6502.py is a starting point; a disassembler is new work.

Recommended:
- One central instruction description for mnemonic, encoding, operands,
  length, profile and register/flag effects.
- Derive shared decoder/assembler/disassembler metadata from that description;
  execution semantics remain implemented in C.
- Explicit legacy and lab profiles; do not silently reinterpret existing bytes.
- Range-check operands, support symbols/listings and eventually bank-aware output.
- Virtual disassembly for a CPU/thread's current mapping and physical bank views.
- Capture executed bytes and the mapping in traces: current memory may differ
  later due to banking or self-modifying code.
- Byte-exact supported-instruction round trips:
  assembly -> machine code -> disassembly -> machine code.

The latest discussion proposed defining the instruction format and tool support
before implementing the new opcodes. That can fit before the banking/opcode phase;
it does not require blocking the foundational CPU-context refactor.

No binary opcode values have been selected.

## Completed before phase 1

- Copied the original current working tree, including uncommitted source changes.
- Excluded old .git and top-level executables/object/debug artifacts.
- Preserved LICENSE and the bundled curses library.
- Initialized a new local Git repository, without commits or remote.
- Created docs/ARCHITECTURE.md, BASELINE.md, UPSTREAM-README.md and COPY-MANIFEST.json.
- Verified hashes of all 27 copied files.
- Built the headless emulator; existing self-tests passed.
- Ran Conway for 1,000,000 instructions successfully.
- Existing cycle counter: 2,981,297; not a cycle-accuracy claim.
- Interactive curses build remains unverified.

## Authorized phase 1 and actual current status

The user said: "Ok, begin maar aan fase 1".
This was interpreted and announced as architecture phase 1:
replace the global CPU state with explicit CPU instances.

Work began in a temporary staging directory because this conversation belongs
to the Game of Life workspace and writes to the lab require scoped escalation:

C:\Users\JWvdV\AppData\Local\Temp\6502-lab-phase1-work

The actual lab src/ files still contain the original baseline. No phase 1 changes
have been promoted into those files. Phase 1 is NOT complete.

A baseline trace was successfully generated before refactoring:
- 1,000,000 Conway instructions.
- 1,001 checkpoints (initial state, then every 1,000 instructions).
- Registers, current opcode, cycle counter, stack depth and a full-memory FNV-1a hash.
- Final checkpoint:
  1000000 00 00 0C FD 22 D0D9 85 2981297 2 6A539D18F476C9F2

A draft modifies staged 6502cpu.c/.h and 6502test.c:
- Pass cpu_t* through public CPU functions, handlers and address resolution.
- Bind a caller-owned flat 64 KiB memory buffer to each CPU.
- Move stack_depth and status-request state into the instance.
- Replace CPU-side UI formatting with an observer callback in the frontend.
- Run CPU self-tests with a local CPU and isolated memory.

This draft has NOT passed compilation or review. Its first compile currently
fails because an overly broad textual replacement changed an include to:
  #include "6502cpu->h"
It must be corrected and the entire generated diff reviewed for similar errors.
Do not treat the archived draft as ready code.

The temporary draft and baseline trace are preserved in phase1-draft.zip next
to this document. The archive contains source/test material, not build binaries.
It is an incomplete recovery aid, not an implementation to copy blindly.

## Next implementation steps

1. Read ARCHITECTURE.md and inspect the actual lab working tree for newer changes.
2. Review/fix or redo the staged context refactor; avoid global active_cpu.
3. Keep existing instruction semantics unchanged during this phase.
4. Explicitly document the interim flat-memory binding; banking stays a later phase.
5. Compile with Clang and run existing self-tests.
6. Add focused tests with two independent CPU instances, reset isolation,
   stack/register/memory isolation and independent status observers.
7. Compare the refactored Conway trace with the saved baseline checkpoints.
8. Check both dispatch paths and syntax-check the interactive frontend.
9. Only then promote reviewed files, update documentation and report phase 1 done.

Architecture sequence: CPU contexts -> central memory layer -> single-CPU undo ->
banking/opcodes with assembler/disassembler support -> two CPUs/mailbox ->
cooperative threads/events -> richer visual debugger.

## Task location

The 6502-lab project is present in Codex, but available task tools do not expose
moving this conversation into another project. This file transfers the context
to the lab without claiming to have moved the conversation.

Suggested continuation prompt in a task opened in 6502-lab:
"Read docs/CHAT-SUMMARY.md and docs/ARCHITECTURE.md. Continue and complete the
authorized phase 1 CPU-context refactor. Review the archived draft carefully;
it is incomplete and has not compiled. Preserve the original emulator."




