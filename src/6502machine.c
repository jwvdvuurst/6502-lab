#include <stdlib.h>
#include "6502machine.h"
typedef struct { WORD block, offset; BYTE old_value; } memory_delta_t;
typedef struct {
    cpu_t cpu;
    address_space_t space;
    memory_delta_t writes[MACHINE_MAX_WRITES];
    size_t write_count;
} history_entry_t;
struct machine_t {
    cpu_t *cpu;
    address_space_t *space;
    physical_memory_t *memory;
    history_entry_t *history;
    size_t capacity, start, count, cursor;
    unsigned long long oldest;
    bool active, restoring, overflow;
    history_entry_t pending;
};
static bool journal_write(const physical_memory_t *memory, WORD block, WORD offset,
                          BYTE old_value, BYTE new_value, void *data) {
    machine_t *m = data;
    (void)memory; (void)new_value;
    if (m->restoring) return true;
    if (!m->active) return false;
    if (m->pending.write_count == MACHINE_MAX_WRITES) {
        m->overflow = true;
        return false;
    }
    m->pending.writes[m->pending.write_count++] = (memory_delta_t){block, offset, old_value};
    return true;
}
machine_t *machine_create(cpu_t *cpu, size_t capacity) {
    if (!cpu || !address_space_valid(cpu->address_space) || !capacity ||
        capacity > MACHINE_MAX_HISTORY || cpu->address_space->physical->write_guard) return NULL;
    machine_t *m = calloc(1, sizeof *m);
    if (!m) return NULL;
    m->history = calloc(capacity, sizeof *m->history);
    if (!m->history) { free(m); return NULL; }
    m->cpu = cpu; m->space = cpu->address_space; m->memory = m->space->physical;
    m->capacity = capacity;
    m->memory->write_guard = journal_write; m->memory->guard_data = m;
    return m;
}
void machine_destroy(machine_t *m) {
    if (!m || m->active) return;
    m->memory->write_guard = NULL; m->memory->guard_data = NULL;
    free(m->history); free(m);
}
const cpu_t *machine_cpu(const machine_t *m) { return m ? m->cpu : NULL; }
static void restore(machine_t *m, const history_entry_t *entry) {
    /* Physical identities remain correct even if the action changed mappings.
     * Restore repeated writes in reverse order; suppress forward notifications. */
    memory_write_observer_t observer = m->memory->write_observer;
    m->memory->write_observer = NULL; m->restoring = true;
    for (size_t i = entry->write_count; i > 0; i--) {
        memory_delta_t delta = entry->writes[i-1];
        memory_write8(m->memory, delta.block, delta.offset, delta.old_value);
    }
    m->restoring = false; m->memory->write_observer = observer;
    *m->space = entry->space;
    *m->cpu = entry->cpu;
}
machine_result_t machine_transaction(machine_t *m, machine_action_t action, void *data) {
    if (!m || !action || m->active) return MACHINE_INVALID;
    m->pending.cpu = *m->cpu; m->pending.space = *m->space;
    m->pending.write_count = 0; m->overflow = false; m->active = true;
    bool ok = action(m->cpu, data);
    m->active = false;
    if (!ok || m->overflow) {
        restore(m, &m->pending);
        return m->overflow ? MACHINE_WRITE_LIMIT : MACHINE_ACTION_FAILED;
    }
    /* A successful new instruction/edit replaces the future after undo.
     * Failed actions leave both retained history and its cursor untouched. */
    m->count = m->cursor;
    if (m->count == m->capacity) {
        m->start = (m->start + 1) % m->capacity;
        m->oldest++; m->count--; m->cursor--;
    }
    m->history[(m->start + m->count) % m->capacity] = m->pending;
    m->count++; m->cursor = m->count;
    return MACHINE_OK;
}
static bool tick_action(cpu_t *cpu, void *data) { (void)data; return cpu_tick(cpu); }
machine_result_t machine_step(machine_t *m) { return machine_transaction(m, tick_action, NULL); }
machine_result_t machine_step_back(machine_t *m) {
    if (!m || m->active) return MACHINE_INVALID;
    if (!m->cursor) return MACHINE_NO_HISTORY;
    restore(m, &m->history[(m->start + m->cursor - 1) % m->capacity]);
    m->cursor--;
    return MACHINE_OK;
}
typedef struct { WORD address; BYTE value; } poke_t;
static bool poke_action(cpu_t *cpu, void *data) {
    poke_t *poke = data;
    return bus_write8(cpu->address_space, poke->address, poke->value);
}
machine_result_t machine_poke(machine_t *m, WORD address, BYTE value) {
    poke_t poke = {address, value};
    return machine_transaction(m, poke_action, &poke);
}
static bool pc_action(cpu_t *cpu, void *data) { cpu->PC = *(WORD *)data; return true; }
machine_result_t machine_set_pc(machine_t *m, WORD value) {
    return machine_transaction(m, pc_action, &value);
}
unsigned long long machine_position(const machine_t *m) { return m ? m->oldest + m->cursor : 0; }
unsigned long long machine_oldest(const machine_t *m) { return m ? m->oldest : 0; }
unsigned long long machine_newest(const machine_t *m) { return m ? m->oldest + m->count : 0; }
size_t machine_capacity(const machine_t *m) { return m ? m->capacity : 0; }
const char *machine_result_text(machine_result_t result) {
    switch (result) {
        case MACHINE_OK: return "OK";
        case MACHINE_NO_HISTORY: return "Oldest retained state reached";
        case MACHINE_ACTION_FAILED: return "Action failed; state rolled back";
        case MACHINE_WRITE_LIMIT: return "Transaction write limit exceeded; state rolled back";
        default: return "Invalid machine operation";
    }
}
