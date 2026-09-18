# 6502emulator
An emulator for the 6502 CPU

Build with `make` and run `./6502test.exe programs/conway_life_6502.bin`
(omit `.exe` on Unix). The interactive build requires a compiler compatible
with the curses library; the bundled `lib/pdcurses.a` requires MinGW on Windows.

The CPU runs unrestricted. Keyboard polling and screen updates are limited to
approximately 60 Hz, with clock checks every 1,024 instructions. Only changed
screen cells are submitted to curses. The status panel samples one instruction
per frame; it is not a trace of every instruction. There is no 1 MHz throttle.

For CPU benchmarks without terminal I/O, use:

```
make headless
./6502headless.exe --self-test
./6502headless.exe --ticks 10000000 --dump-screen programs/conway_life_6502.bin
```

`--ticks` counts instructions, not clock cycles. Timing excludes loading and
startup tests. The cycle table is incomplete and does not fully account for
page crossing/branch penalties, so reported cycles are not an accurate MHz
measurement. The headless executable supports only headless operation.

In a local Clang `-O2` comparison using the same no-op terminal backend,
10 million Conway instructions took 6.245 seconds with per-instruction status
formatting and 0.095 seconds with sampled status (about 65x faster). Screen RAM
output and cycle totals matched. This isolates formatting cost, not terminal
rendering; interactive performance depends on the console. CPU/memory self-tests
passed, and all interactive sources passed compiler syntax/warning checks.
The interactive binary still needs rebuilding with a compatible toolchain.
