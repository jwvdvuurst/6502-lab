#ifndef CPU_6502MACHINE_H
#define CPU_6502MACHINE_H
#include "6502cpu.h"
typedef struct machine_t machine_t;
enum { MACHINE_MAX_WRITES = 16, MACHINE_MAX_HISTORY = 65536 };
typedef enum {
    MACHINE_OK, MACHINE_NO_HISTORY, MACHINE_ACTION_FAILED,
    MACHINE_WRITE_LIMIT, MACHINE_INVALID
} machine_result_t;
typedef bool (*machine_action_t)(cpu_t *cpu, void *data);
/* Borrows one initialized CPU, its address space and physical memory. Allocates
 * a bounded history (1..65536 entries). Returns NULL on invalid setup/allocation.
 * Only one owner per physical memory; an existing write guard is rejected.
 * Use transactions for ALL mutations while attached. Raw memory writes are
 * rejected outside transactions. Do not change bindings or observer callbacks,
 * reinitialize objects, call cpu_tick/reset directly or edit mappings outside a
 * transaction. Destroy the machine before releasing borrowed objects. */
machine_t *machine_create(cpu_t *cpu, size_t history_capacity);
void machine_destroy(machine_t *machine);
const cpu_t *machine_cpu(const machine_t *machine);
machine_result_t machine_step(machine_t *machine);
machine_result_t machine_step_back(machine_t *machine);
/* Action may change CPU execution state, mappings and up to 16 physical bytes
 * (repeated writes count separately). No I/O or rebinding. False return or write
 * overflow rolls back completely. No allocation occurs during transactions.
 * Observers are notification-only; external effects are not reversible. */
machine_result_t machine_transaction(machine_t *machine, machine_action_t action, void *data);
machine_result_t machine_poke(machine_t *machine, WORD address, BYTE value);
machine_result_t machine_set_pc(machine_t *machine, WORD value);
unsigned long long machine_position(const machine_t *machine);
unsigned long long machine_oldest(const machine_t *machine);
unsigned long long machine_newest(const machine_t *machine);
size_t machine_capacity(const machine_t *machine);
const char *machine_result_text(machine_result_t result);
#endif
