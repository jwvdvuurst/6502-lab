/* No terminal backend: for CPU validation and reproducible headless benchmarks.
 * Only use the resulting executable with --self-test or --ticks. */
#include <stdio.h>
#include "6502io.h"

bool write_statmessage(const char *message) { (void)message; return true; }
bool backspace_pressed(void) { return false; }
bool display(void) { return false; }
bool display_test(framebuffer_t *framebuffer) {
    (void)framebuffer;
    fprintf(stderr, "This build requires --self-test or --ticks.\n");
    return false;
}
bool close_io(void) { return true; }

