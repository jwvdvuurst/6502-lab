#ifndef CPU_6502DEBUGGER_H
#define CPU_6502DEBUGGER_H
#include <stdio.h>
#include "6502machine.h"
#include "6502framebuffer.h"
/* Small stream-based debugger; no curses or host input reaches guest execution. */
bool debugger_run(machine_t *machine, const framebuffer_t *framebuffer, FILE *input, FILE *output);
#endif
