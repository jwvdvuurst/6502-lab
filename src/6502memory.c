#include <assert.h>
#include "6502memory.h"
static bool memory_valid(const physical_memory_t *memory) {
    return memory && memory->bytes && memory->block_count > 0 && memory->block_count <= 65536;
}
bool memory_init(physical_memory_t *memory, BYTE *storage, size_t size) {
    if (!memory || !storage || !size || size % MEMORY_BLOCK_SIZE ||
        size / MEMORY_BLOCK_SIZE > 65536) return false;
    *memory = (physical_memory_t){storage, size / MEMORY_BLOCK_SIZE, NULL, NULL, NULL, NULL};
    return true;
}
bool address_space_init(address_space_t *space, physical_memory_t *memory) {
    if (!space || !memory_valid(memory) || memory->block_count < MEMORY_WINDOW_COUNT) return false;
    space->physical = memory;
    for (size_t i = 0; i < MEMORY_WINDOW_COUNT; i++) space->blocks[i] = (WORD)i;
    return true;
}
bool address_space_valid(const address_space_t *space) {
    if (!space || !memory_valid(space->physical)) return false;
    for (size_t i = 0; i < MEMORY_WINDOW_COUNT; i++)
        if (space->blocks[i] >= space->physical->block_count) return false;
    return true;
}
bool address_space_map(address_space_t *space, size_t window, size_t block) {
    if (!address_space_valid(space) || window >= MEMORY_WINDOW_COUNT ||
        block >= space->physical->block_count) return false;
    space->blocks[window] = (WORD)block;
    return true;
}
BYTE memory_read8(const physical_memory_t *memory, WORD block, WORD offset) {
    assert(memory_valid(memory) && block < memory->block_count && offset < MEMORY_BLOCK_SIZE);
    return memory->bytes[(size_t)block * MEMORY_BLOCK_SIZE + offset];
}
bool memory_write8(physical_memory_t *memory, WORD block, WORD offset, BYTE value) {
    assert(memory_valid(memory) && block < memory->block_count && offset < MEMORY_BLOCK_SIZE);
    size_t index = (size_t)block * MEMORY_BLOCK_SIZE + offset;
    if (memory->write_guard && !memory->write_guard(memory, block, offset,
        memory->bytes[index], value, memory->guard_data)) return false;
    if (memory->write_observer)
        memory->write_observer(memory, block, offset, memory->bytes[index], value, memory->observer_data);
    memory->bytes[index] = value;
    return true;
}
BYTE bus_read8(const address_space_t *space, WORD address) {
    assert(space && space->physical);
    return memory_read8(space->physical, space->blocks[address >> 10], address & 0x03FF);
}
bool bus_write8(address_space_t *space, WORD address, BYTE value) {
    assert(space && space->physical);
    return memory_write8(space->physical, space->blocks[address >> 10], address & 0x03FF, value);
}
WORD bus_read16(const address_space_t *space, WORD address) {
    BYTE lo = bus_read8(space, address);
    BYTE hi = bus_read8(space, (WORD)(address + 1));
    return (WORD)(lo | (hi << 8));
}
bool bus_write16(address_space_t *space, WORD address, WORD value) {
    return bus_write8(space, address, (BYTE)value) &&
        bus_write8(space, (WORD)(address + 1), (BYTE)(value >> 8));
}
bool memory_test(void) {
    BYTE storage[65536] = {0};
    physical_memory_t memory;
    address_space_t space;
    const WORD values[8] = {0x0123,0x4567,0x89AB,0xCDEF,0x1234,0x5678,0x9ABC,0xDEF0};
    if (!memory_init(&memory, storage, sizeof storage) || !address_space_init(&space, &memory)) return false;
    for (unsigned int address = 0; address < 65536; address++) {
        bus_write8(&space, (WORD)address, 0xFF);
        if (bus_read8(&space, (WORD)address) != 0xFF) return false;
        bus_write16(&space, (WORD)address, values[address % 8]);
        if (bus_read16(&space, (WORD)address) != values[address % 8]) return false;
    }
    return true;
}

