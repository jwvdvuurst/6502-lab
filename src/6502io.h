#ifndef CPU_6502IO_H
#define CPU_6502IO_H

#include "6502framebuffer.h"

bool clear_display(void);

bool write_string(BYTE x, BYTE y,const char* text);
bool scroll_display(BYTE l);
bool write_statmessage(const char* message);

bool display(void);
bool init_io(framebuffer_t *framebuffer);
bool close_io(void);

bool backspace_pressed(void);

bool display_test(framebuffer_t *framebuffer);

#endif // CPU_6502IO_H

