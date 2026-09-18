#ifndef CPU_6502FRAMEBUFFER_H
#define CPU_6502FRAMEBUFFER_H
#include "6502memory.h"
enum { FRAMEBUFFER_WIDTH = 40, FRAMEBUFFER_HEIGHT = 25, FRAMEBUFFER_CELLS = 1000 };
typedef struct {
    physical_memory_t *physical;
    WORD block;
} framebuffer_t;
/* Fixed physical block, independent of subsequent CPU mapping changes.
 * Binding neither clears nor writes the block. Failed binding leaves it intact. */
bool framebuffer_init(framebuffer_t *fb, physical_memory_t *memory, size_t block);
BYTE framebuffer_read8(const framebuffer_t *fb, WORD offset);
bool framebuffer_clear(framebuffer_t *fb);
bool framebuffer_write_char(framebuffer_t *fb, BYTE x, BYTE y, BYTE value);
bool framebuffer_write_string(framebuffer_t *fb, BYTE x, BYTE y, const char *text);
bool framebuffer_scroll(framebuffer_t *fb, BYTE lines);
#endif
