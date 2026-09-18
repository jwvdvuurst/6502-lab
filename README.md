# 6502 Lab

Experimenteel vervolgproject op de bestaande 6502emulator: onderzoek naar
1 KiB-banking, coöperatieve threads, meerdere virtuele CPU's met gedeeld geheugen
en een globale tijdmachine.

**Status:** fase 1–3 zijn geverifieerd: CPU-contexten, geheugenbus, fysiek gebonden
framebuffer, undo voor één CPU en een kleine tekstdebugger. Gast-banking volgt later.

- [Debugger: starten en commando's](docs/DEBUGGER.md)
- [Fase 3: undo en verificatie](docs/PHASE3.md)
- [Fase 2: implementatie en verificatie](docs/PHASE2.md)
- [Inbouwplan en ontwerpkeuzes](docs/ARCHITECTURE.md)
- [Herkomst en baselinevalidatie](docs/BASELINE.md)
- [Oorspronkelijke README](docs/UPSTREAM-README.md)
- [Kopie-manifest met SHA-256-hashes](docs/COPY-MANIFEST.json)

## Uitvoeren

Met Clang en make beschikbaar:

```sh
make headless
./6502headless.exe --self-test
./6502headless.exe --ticks 1000000 --dump-screen programs/conway_life_6502.bin
```

Op Windows zonder Clang in PATH of make:

```powershell
& 'C:\Program Files\LLVM\bin\clang.exe' -Isrc -std=c11 -Wall -Wextra -Wpedantic -O2 -g -o 6502headless.exe src/6502test.c src/6502cpu.c src/6502memory.c src/6502framebuffer.c src/6502machine.c src/6502debugger.c tools/headless_io.c
.\6502headless.exe --self-test
.\6502headless.exe --ticks 1000000 --dump-screen programs/conway_life_6502.bin
```

Op Unix heet het programma 6502headless, zonder .exe. De interactieve curses-build
heeft een passende curses-toolchain nodig; de meegeleverde pdcurses.a is voor
MinGW. Die interactieve build is in deze kopie nog niet opnieuw geverifieerd.

De tick-eenheid is één instructie. De bestaande cyclustelling is niet nauwkeurig
genoeg om MHz- of hardwaretimingclaims op te baseren.

## Herkomst

Overgenomen uit de actuele lokale werkboom van 6502emulator, inclusief
niet-gecommitte bronwijzigingen. De oude .git-directory en oude uitvoerbare
bestanden/object-/debugbestanden zijn niet meegenomen. De oorspronkelijke
LICENSE en de benodigde pdcurses-bibliotheek zijn behouden.


## Regressietests (PowerShell 7)

```powershell
./tools/test-phase3.ps1
```

Bouwt en test fase 3 met en zonder optimalisatie en voert ook de fase 1/2-tests,
Conway-tracevergelijking en frontend-syntaxcheck uit. Iedere compiler/test heeft
een limiet van 30 seconden. Resultaten staan onder `build/phase1`, `build/phase2` en `build/phase3`.
De actuele headless testbuild staat in `build/phase3/6502headless.exe`; bouw met het
bovenstaande commando opnieuw als je de executable in de hoofdmap gebruikt.


Start de debugger na het bouwen vanuit de projectmap:

```powershell
./build/phase3/6502headless.exe --debugger programs/conway_life_6502.bin
```

Gebruik `help`, `step`, `back`, `regs`, `mem c000 64`, `run 1000`, `screen` en `quit`.
Zie [DEBUGGER.md](docs/DEBUGGER.md) voor breakpoints, edits en historie-instellingen.
