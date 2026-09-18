#include <assert.h>
#include <string.h>
#include "6502framebuffer.h"
static bool framebuffer_valid(const framebuffer_t *fb) {
    return fb && fb->physical && fb->physical->bytes &&
        fb->physical->block_count <= 65536 && fb->block < fb->physical->block_count;
}
bool framebuffer_init(framebuffer_t *fb, physical_memory_t *memory, size_t block) {
    if (!fb || !memory || !memory->bytes || memory->block_count > 65536 ||
        block >= memory->block_count) return false;
    *fb = (framebuffer_t){memory, (WORD)block};
    return true;
}
BYTE framebuffer_read8(const framebuffer_t *fb, WORD offset) {
    assert(framebuffer_valid(fb));
    return memory_read8(fb->physical, fb->block, offset);
}
bool framebuffer_clear(framebuffer_t *fb) {
    if (!framebuffer_valid(fb)) return false;
    for (WORD i = 0; i < MEMORY_BLOCK_SIZE; i++)
        if (!memory_write8(fb->physical, fb->block, i, 0x20)) return false;
    return true;
}
bool framebuffer_write_char(framebuffer_t *fb, BYTE x, BYTE y, BYTE value) {
    if (!framebuffer_valid(fb) || x >= FRAMEBUFFER_WIDTH || y >= FRAMEBUFFER_HEIGHT) return false;
    return memory_write8(fb->physical, fb->block, (WORD)(y * FRAMEBUFFER_WIDTH + x), value);
}
bool framebuffer_write_string(framebuffer_t *fb, BYTE x, BYTE y, const char *text) {
    if (!framebuffer_valid(fb) || !text || x >= FRAMEBUFFER_WIDTH || y >= FRAMEBUFFER_HEIGHT) return false;
    size_t start = y * FRAMEBUFFER_WIDTH + x;
    size_t length = strlen(text);
    if (length > FRAMEBUFFER_CELLS - start) return false;
    for (size_t i = 0; i < length; i++)
        if (!memory_write8(fb->physical, fb->block, (WORD)(start + i), (BYTE)text[i])) return false;
    return true;
}
bool framebuffer_scroll(framebuffer_t *fb, BYTE lines) {
    if (!framebuffer_valid(fb)) return false;
    /* Keep the original frontend's >=24 clear behavior. */
    if (lines >= 24) return framebuffer_clear(fb);
    while (lines--) {
        for (WORD i = 0; i < 960; i++)
            if (!memory_write8(fb->physical, fb->block, i, framebuffer_read8(fb, (WORD)(i + 40)))) return false;
        for (WORD i = 960; i < FRAMEBUFFER_CELLS; i++)
            if (!memory_write8(fb->physical, fb->block, i, 0x20)) return false;
    }
    return true;
}

