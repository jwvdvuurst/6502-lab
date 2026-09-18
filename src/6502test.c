#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "6502types.h"
#include "6502memory.h"
#include "6502cpu.h"
#include "6502io.h"
#include "6502debugger.h"

static void show_cpu_status(const cpu_t *cpu, const char *mode,
                            const char *instruction, void *user_data) {
    (void)user_data;
    char message[160];
    snprintf(message, sizeof(message),
        "OC %s: IN: %02X pc: %04X am: %s\tsp: %02X ps: %02X acc: %02X rx: %02X ry: %02X",
        instruction, cpu->current_instruction, cpu->PC, mode,
        cpu->SP, cpu->P, cpu->A, cpu->X, cpu->Y);
    write_statmessage(message);
}

static double elapsed_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER now, frequency;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
#endif
}

static bool loop(cpu_t *cpu) {
    bool ok = true;
    bool do_exit = false;

    double next_frame = elapsed_seconds();
    while(ok && !do_exit) {
        /* Amortize clock checks; run the CPU unrestricted between UI frames. */
        for (int i = 0; ok && i < 1024; i++) ok = cpu_tick(cpu);
        double now = elapsed_seconds();
        if (now >= next_frame) {
            do_exit = backspace_pressed();
            cpu_request_status(cpu);
            ok = ok && display();
            next_frame = now + 1.0 / 60.0;
        }
    }
    return ok;
}

static bool run_ticks(cpu_t *cpu, unsigned long long max_ticks) {
    bool ok = true;
    unsigned long long executed = 0;
    double start = elapsed_seconds();

    for (unsigned long long i = 0; ok && (i < max_ticks); i++) {
        ok = ok && cpu_tick(cpu);
        if (ok) executed++;
    }

    double seconds = elapsed_seconds() - start;
    fprintf(stderr, "Executed %llu instructions in %.6f s (%.3f million instructions/s); cycles=%llu\n",
            executed, seconds, seconds > 0 ? executed / seconds / 1e6 : 0,
            cpu_get_cycles(cpu));
    return ok;
}

static void dump_screen(const framebuffer_t *framebuffer) {
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 40; x++) {
            BYTE ch = framebuffer_read8(framebuffer, (WORD)((y * 40) + x));
            putchar((ch >= 0x20) && (ch <= 0x7E) ? ch : '.');
        }
        putchar('\n');
    }
}

static bool load_memory(address_space_t *space) {
    BYTE program[40] = {0x20,0x09,0x06,0x20,0x0c,0x06,0x20,0x12,0x06,0xa2,0x00,0x60,0xe8,0xe0,0x05,0xd0,0xfb,0x60,0x00,0xa9,0x03,0x4c,0x08,0x06,0x00,0x00,0x00,0x8d,0x00,0x02,0xa9,0xc0,0xaa,0xe8,0x69,0xc4,0x00,0xea,0xea,0xea};
    WORD address = 0x0000;

    srand(time(0));

    bus_write8(space, 0x0000,0x60);
    bus_write8(space, 0x0001,0x6C);
    bus_write16(space, 0x0002,0xD000);
    bus_write8(space, 0x0004,0x50);
    bus_write16(space, 0x0005,0x0400);
    bus_write8(space, 0x0007,0x70);
    bus_write16(space, 0x0008,0xE000);
    bus_write8(space, 0x000A,0x90);
    bus_write16(space, 0x000B,0xF000);
    bus_write8(space, 0x000D,0xB0);
    bus_write16(space, 0x000E,0x0000);

    address = 0x0010;

    while (address < 0xA200) {
        bus_write8(space, address,0xE8);
        if (address%3) bus_write8(space, address++,0x00);
        if (address%5) bus_write8(space, address++,0xAE);
        if (address%7) bus_write8(space, address++,0x6B);
        if (address%11) bus_write8(space, address++,0xC8);
        if (address%13) bus_write8(space, address++,0xE8);
        if (address%17) bus_write8(space, address++,0x4C);
        if (address%23) bus_write8(space, address++,0x50);
        bus_write8(space, address++,0x00);
        bus_write8(space, address++,(BYTE)rand()%256);
    }

    bus_write8(space, 0xA200,0x60);
    bus_write8(space, 0xA201,0x6C);
    bus_write16(space, 0xA202,0xD000);
    bus_write8(space, 0xA204,0x50);
    bus_write16(space, 0xA205,0x0400);
    bus_write8(space, 0xA207,0x70);
    bus_write16(space, 0xA208,0xE000);
    bus_write8(space, 0xA20A,0x90);
    bus_write16(space, 0xA20B,0xF000);
    bus_write8(space, 0xA20D,0xB0);
    bus_write16(space, 0xA20E,0x0000);

    address = 0xA210;

    while (address < 0xC000) {
        bus_write8(space, address,0xE8);
        if (address%3) bus_write8(space, address++,0x00);
        if (address%5) bus_write8(space, address++,0xAE);
        if (address%7) bus_write8(space, address++,0x6B);
        if (address%11) bus_write8(space, address++,0xC8);
        if (address%13) bus_write8(space, address++,0xE8);
        if (address%17) bus_write8(space, address++,0x4C);
        if (address%23) bus_write8(space, address++,0x50);
        bus_write8(space, address++,0x00);
        bus_write8(space, address++,(BYTE)rand()%256);
    }



    address = 0xD000;

    while(address < 0xFFFF) {
        bus_write8(space, address,program[address%40]);
        address++;
    }

    bus_write16(space, 0xFFFC, 0xD000);

    return true;
}

static bool parse_address(const char *text, WORD *address) {
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if ((text == end) || (*end != '\0') || (value > 0xFFFFUL)) {
        return false;
    }
    *address = (WORD)value;
    return true;
}

static bool has_extension(const char *path, const char *extension) {
    const char *dot = strrchr(path, '.');
    if (dot == NULL) return false;
    while ((*dot != '\0') && (*extension != '\0')) {
        if (tolower((unsigned char)*dot) != tolower((unsigned char)*extension)) {
            return false;
        }
        dot++;
        extension++;
    }
    return (*dot == '\0') && (*extension == '\0');
}

static bool load_binary_file(address_space_t *space, const char *path, WORD requested_address, bool address_is_explicit) {
    if (has_extension(path, ".asm")) {
        fprintf(stderr, "Assembly source must be assembled first. Try: make programs/conway_life_6502.bin\n");
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Unable to open program file: %s\n", path);
        return false;
    }

    WORD load_address = requested_address;
    if (!address_is_explicit && has_extension(path, ".prg")) {
        int lo = fgetc(file);
        int hi = fgetc(file);
        if ((lo == EOF) || (hi == EOF)) {
            fprintf(stderr, "Program file is too short to contain a PRG load address: %s\n", path);
            fclose(file);
            return false;
        }
        load_address = (WORD)((hi << 8) | lo);
    }

    WORD address = load_address;
    int byte = 0;
    while ((byte = fgetc(file)) != EOF) {
        bus_write8(space, address++, (BYTE)byte);
        if (address == 0x0000) {
            break;
        }
    }
    fclose(file);

    bus_write16(space, 0xFFFC, load_address);
    fprintf(stderr, "Loaded %s at $%04X; reset vector set to $%04X\n", path, load_address, load_address);
    return true;
}

int main(int argc, char **argv) {
    cpu_t cpu;
    BYTE storage[65536];
    physical_memory_t physical;
    address_space_t space;
    framebuffer_t framebuffer;
    if (!memory_init(&physical, storage, sizeof storage) ||
        !address_space_init(&space, &physical) ||
        !framebuffer_init(&framebuffer, &physical, 0xC000 / MEMORY_BLOCK_SIZE)) return 1;
    /* Explicit legacy startup fill; self-tests no longer initialize guest memory. */
    for (unsigned i = 0; i < 65536; i++) bus_write8(&space, (WORD)i, 0xEA);
    bool ok = true;
    bool do_exit = false;
    bool program_loaded = false;
    bool self_test_only = false;
    bool headless = false;
    bool debugging = false, history_given = false, ticks_given = false;
    size_t history_capacity = 4096;
    bool dump_screen_after_run = false;
    unsigned long long headless_ticks = 0ULL;
    int argi = 1;

    while (argi < argc) {
        if (strcmp(argv[argi], "--debugger") == 0) {
            debugging = true; headless = true; argi++;
        } else if (strcmp(argv[argi], "--history") == 0) {
            char *end = NULL;
            if (argi+1 >= argc || argv[argi+1][0] == '-' || argv[argi+1][0] == '+') {
                fprintf(stderr,"--history requires a count from 1 to 65536\n"); return 1;
            }
            unsigned long count = strtoul(argv[argi+1], &end, 10);
            if (end == argv[argi+1] || *end || !count || count > MACHINE_MAX_HISTORY) {
                fprintf(stderr,"--history requires a count from 1 to 65536\n"); return 1;
            }
            history_capacity = (size_t)count; history_given = true; argi += 2;
        } else if (strcmp(argv[argi], "--self-test") == 0) {
            self_test_only = true;
            argi++;
        } else if (strcmp(argv[argi], "--ticks") == 0) {
            char *end = NULL;
            if ((argi + 1) >= argc) {
                fprintf(stderr, "--ticks requires a count\n");
                return 1;
            }
            ticks_given = true;
            headless_ticks = strtoull(argv[argi + 1], &end, 0);
            if ((argv[argi + 1] == end) || (*end != '\0')) {
                fprintf(stderr, "Invalid tick count: %s\n", argv[argi + 1]);
                return 1;
            }
            headless = true;
            argi += 2;
        } else if (strcmp(argv[argi], "--dump-screen") == 0) {
            dump_screen_after_run = true;
            headless = true;
            argi++;
        } else {
            break;
        }
    }

    if ((history_given && !debugging) ||
        (debugging && (ticks_given || dump_screen_after_run || self_test_only || argi >= argc))) {
        fprintf(stderr,"Usage: --debugger [--history 1..65536] program.bin [load_address]\n");
        return 1;
    }
    ok = ok && memory_test();

    if (!ok) {
        fprintf(stderr,"Memory test failed, do_exit");
        do_exit = true;
    }

    ok = ok && cpu_test();

    if (!ok && !do_exit) {
        fprintf(stderr,"CPU test failed, do_exit");
        do_exit = true;
    }

    if (self_test_only && !do_exit) {
        fprintf(stderr, "Self-tests passed\n");
        exit(0);
    }

    if (!headless) {
        ok = ok && display_test(&framebuffer);
        if (!ok && !do_exit) {
            fprintf(stderr,"Display test failed, do_exit");
            do_exit = true;
        }
    }

    if (argi < argc) {
        WORD load_address = 0xD000;
        bool address_is_explicit = false;
        if ((argi + 1) < argc) {
            ok = ok && parse_address(argv[argi + 1], &load_address);
            if (!ok) {
                fprintf(stderr, "Invalid load address: %s\n", argv[argi + 1]);
                do_exit = true;
            }
            address_is_explicit = true;
        }
        if (!do_exit) {
            ok = ok && load_binary_file(&space, argv[argi], load_address, address_is_explicit);
            program_loaded = ok;
            if (!ok) {
                do_exit = true;
            }
        }
    } else {
        ok = ok && load_memory(&space);
        program_loaded = ok;
    }

    if (program_loaded) {
        ok = ok && cpu_init(&cpu, &space);
        if (!ok) do_exit = true;
        cpu.status_callback = show_cpu_status;
    }

    if (!do_exit) {
        if (debugging) {
            machine_t *machine = machine_create(&cpu, history_capacity);
            if (!machine) { fprintf(stderr,"Cannot create debugger history\n"); ok = false; }
            else {
                ok = debugger_run(machine, &framebuffer, stdin, stdout);
                machine_destroy(machine);
            }
        } else if (headless) {
            ok = ok && run_ticks(&cpu, headless_ticks);
            if (dump_screen_after_run) {
                dump_screen(&framebuffer);
            }
        } else {
            ok = ok && loop(&cpu);
        }
    }

    if (!ok && !do_exit) {
        fprintf(stderr,"Error occurred during loop");
        do_exit = true;
    }

    close_io();

    if (do_exit) exit(1);

    exit(0);
}


