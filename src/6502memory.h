#ifndef CPU_6502MEMORY_H
#define CPU_6502MEMORY_H
#include <stddef.h>
#include "6502types.h"
enum { MEMORY_BLOCK_SIZE = 1024, MEMORY_WINDOW_COUNT = 64 };
typedef struct physical_memory_t physical_memory_t;
/* Synchronous notification BEFORE every physical byte write, including unchanged
 * values. Observers must not mutate memory/mappings or reenter the bus.
 * This observation point is not a transaction or an undo implementation. */
typedef void (*memory_write_observer_t)(const physical_memory_t *memory,
    WORD block, WORD offset, BYTE old_value, BYTE new_value, void *user_data);
/* A transaction guard may veto a write before the observer and storage update.
 * The guard owns failure handling; callers can also inspect the false result. */
typedef bool (*memory_write_guard_t)(const physical_memory_t *memory,
    WORD block, WORD offset, BYTE old_value, BYTE new_value, void *user_data);
struct physical_memory_t {
    BYTE *bytes;
    size_t block_count;
    memory_write_observer_t write_observer;
    void *observer_data;
    memory_write_guard_t write_guard;
    void *guard_data;
};
typedef struct {
    physical_memory_t *physical;
    WORD blocks[MEMORY_WINDOW_COUNT];
} address_space_t;
/* Borrow caller-owned storage, unchanged. Size must be a nonzero multiple of
 * 1 KiB, at most 65536 blocks (16-bit IDs). No allocation or ownership transfer.
 * Storage and bindings must outlive their users. Failed initialization or
 * configuration leaves the destination unchanged. Identity needs >=64 blocks. */
bool memory_init(physical_memory_t *memory, BYTE *storage, size_t size);
bool address_space_init(address_space_t *space, physical_memory_t *memory);
bool address_space_valid(const address_space_t *space);
/* Host configuration only, at instruction boundaries. No guest MAP opcode. */
bool address_space_map(address_space_t *space, size_t window, size_t block);
/* Initialized, valid bindings and in-range physical addresses are preconditions
 * for access, asserted in debug builds. Configure using the functions above,
 * not by editing struct internals. Device/bus faults are a later extension. */
BYTE memory_read8(const physical_memory_t *memory, WORD block, WORD offset);
bool memory_write8(physical_memory_t *memory, WORD block, WORD offset, BYTE value);
BYTE bus_read8(const address_space_t *space, WORD address);
bool bus_write8(address_space_t *space, WORD address, BYTE value);
/* Translate each byte independently, including $FFFF -> $0000 wraparound.
 * CPU-specific zero-page addressing rules stay in the CPU. */
WORD bus_read16(const address_space_t *space, WORD address);
bool bus_write16(address_space_t *space, WORD address, WORD value);
bool memory_test(void);
#endif

