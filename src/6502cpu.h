#ifndef CPU_6502CPU_H
#define CPU_6502CPU_H

#include "6502memory.h"

typedef struct cpu_t cpu_t;
/* Called synchronously before a sampled instruction executes. Read-only observer. */
typedef void (*cpu_status_callback_t)(const cpu_t *cpu, const char *mode,
                                      const char *instruction, void *user_data);

struct cpu_t {
    BYTE A, X, Y, SP, P;
    WORD PC;
    BYTE current_instruction;
    unsigned long long cycles;
    /* Borrowed address space and physical storage must outlive this CPU. */
    address_space_t *address_space;
    unsigned int stack_depth;
    bool status_requested;
    cpu_status_callback_t status_callback;
    void *status_user_data;
};

/* Initialize sequentially before execution; shared opcode tables are initialized once.
 * cpu and address_space must be non-NULL; other operations require an initialized CPU.
 * Reinitialization clears observers. Reset preserves memory binding and observer. */
bool cpu_init(cpu_t *cpu, address_space_t *address_space);
void cpu_reset(cpu_t *cpu);
BYTE cpu_fetch(cpu_t *cpu);
bool cpu_execute(cpu_t *cpu, BYTE instruction);
bool cpu_tick(cpu_t *cpu);
void cpu_request_status(cpu_t *cpu);
unsigned long long cpu_get_cycles(const cpu_t *cpu);
/* Self-tests use an isolated local CPU and memory. */
bool cpu_test(void);

#endif

