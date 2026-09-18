# Phase 1 verification — 13 September 2026

Phase 1 CPU contexts are implemented and verified. The original emulator project is untouched.

## Changes and ownership

- CPU initialization, reset, fetch, execute, tick and cycle/status access take an explicit CPU pointer. Instruction handlers, address resolution and both dispatch routes use that instance.
- Registers, cycles, stack depth and pending status requests belong to cpu_t. There is no global active CPU.
- Each CPU borrows a caller-owned 64 KiB buffer. Keep it alive for the CPU lifetime. Reset reads that buffer's reset vector, clears execution state and pending status, and preserves memory contents and observer binding. Reinitialization clears the observer.
- Shared opcode tables are initialized once; initialize CPUs sequentially. Concurrent host-thread execution is outside this phase.
- Status observers are synchronous and read-only; frontend formatting lives in 6502test.c. Observers must not mutate or reenter execution.
- CPU self-tests use private memory. The CLI still binds its CPU to the legacy loader/display buffer. A centralized bus, stack bus access and explicit framebuffer binding belong to Phase 2.

## Draft repair and behavior preservation

The recovered draft failed at the malformed include `6502cpu->h`; correcting it resolved compilation. Reviewed the context propagation and retained existing instruction semantics, opcode aliases, cycle accounting and stack checks. No new guest instructions were introduced.

The historical CPU self-test left byte values 255..0 in the global stack page. The trace fixture explicitly recreates those bytes before execution so comparison starts from identical memory. Production self-tests now leave guest memory untouched, intentionally removing that incidental startup side effect.

## Validation

Run `./tools/test-phase1.ps1` from PowerShell (optional -Compiler path).

- Clang C11 optimized builds, Wall/Wextra/Wpedantic/Werror: passed. The script suppresses only Microsoft's existing fopen deprecation warning.
- Existing CPU/memory self-tests: passed.
- Two interleaved CPUs: independent initialization, registers, stack, zero page, reset vectors, cycle/reset state and status observers: passed.
- Reset and self-test isolation, null initialization and observer reinitialization: passed.
- Table dispatch, reserved-opcode fallback and existing 0xAB fallback handler: passed.
- Conway: 1,000,000 instructions; all 1,001 register/cycle/stack-depth/full-memory-hash checkpoints match the archived baseline.
- Final checkpoint: `1000000 00 00 0C FD 22 D0D9 85 2981297 2 6A539D18F476C9F2`.
- Interactive frontend and all production C files: syntax check passed.

The interactive executable has not been linked or exercised: bundled pdcurses.a targets MinGW while the available Clang defaults to the MSVC toolchain. Existing tests do not establish complete NMOS6502 conformance or cycle accuracy.

## Remaining work

No remaining Phase 1 acceptance work. Interactive runtime validation remains an explicitly unverified frontend check. Phase 2 introduces the central memory bus and physical blocks; banking, scheduling, threads, undo and ISA/tooling extensions remain later phases.

The original phase1-draft.zip is retained as historical recovery material. Pre-integration source is preserved in phase1-baseline-source.zip. The earlier CHAT-SUMMARY and architecture findings describe the pre-refactor state; this report supersedes their Phase 1 status.
