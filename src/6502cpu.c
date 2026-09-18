#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "6502memory.h"
#include "6502types.h"
#include "6502cpu.h"


/* Register aliases always resolve against the explicitly supplied CPU. */
#define stackpointer (cpu->SP)
#define accumulator (cpu->A)
#define regx (cpu->X)
#define regy (cpu->Y)
#define procstatus (cpu->P)
#define programcounter (cpu->PC)
#define current_instruction (cpu->current_instruction)
#define cycles (cpu->cycles)

static bool initialised = false;
#define stack_depth (cpu->stack_depth)
#define status_requested (cpu->status_requested)

#define CARRY_BIT 0
#define ZERO_BIT 1
#define INTERRUPT_BIT 2
#define DECIMAL_BIT 3
#define BREAK_BIT 4
#define OVERFLOW_BIT 6
#define NEGATIVE_BIT 7

#define CARRY_FLAG      ((procstatus & (0x01 << CARRY_BIT)) != 0x00)
#define ZERO_FLAG       ((procstatus & (0x01 << ZERO_BIT)) != 0x00)
#define INTERRUPT_FLAG  ((procstatus & (0x01 << INTERRUPT_BIT)) != 0x00)
#define DECIMAL_FLAG    ((procstatus & (0x01 << DECIMAL_BIT)) != 0x00)
#define BREAK_FLAG      ((procstatus & (0x01 << BREAK_BIT)) != 0x00)
#define OVERFLOW_FLAG   ((procstatus & (0x01 << OVERFLOW_BIT)) != 0x00)
#define NEGATIVE_FLAG   ((procstatus & (0x01 << NEGATIVE_BIT)) != 0x00)

#define CARRY_SET      procstatus = (procstatus | (0x01 << CARRY_BIT))
#define ZERO_SET       procstatus = (procstatus | (0x01 << ZERO_BIT))
#define INTERRUPT_SET  procstatus = (procstatus | (0x01 << INTERRUPT_BIT))
#define DECIMAL_SET    procstatus = (procstatus | (0x01 << DECIMAL_BIT))
#define BREAK_SET      procstatus = (procstatus | (0x01 << BREAK_BIT))
#define OVERFLOW_SET   procstatus = (procstatus | (0x01 << OVERFLOW_BIT))
#define NEGATIVE_SET   procstatus = (procstatus | (0x01 << NEGATIVE_BIT))

#define CARRY_CLEAR      procstatus &= ~(0x01 << CARRY_BIT)
#define ZERO_CLEAR       procstatus &= ~(0x01 << ZERO_BIT)
#define INTERRUPT_CLEAR  procstatus &= ~(0x01 << INTERRUPT_BIT)
#define DECIMAL_CLEAR    procstatus &= ~(0x01 << DECIMAL_BIT)
#define BREAK_CLEAR      procstatus &= ~(0x01 << BREAK_BIT)
#define OVERFLOW_CLEAR   procstatus &= ~(0x01 << OVERFLOW_BIT)
#define NEGATIVE_CLEAR   procstatus &= ~(0x01 << NEGATIVE_BIT)

/* All guest accesses, including the stack, go through the address-space bus. */
static BYTE cpu_fetch_byte(const cpu_t *cpu, WORD address) {
    return bus_read8(cpu->address_space, address);
}
static WORD cpu_fetch_word(const cpu_t *cpu, WORD address) {
    return bus_read16(cpu->address_space, address);
}
static void cpu_store_byte(cpu_t *cpu, WORD address, BYTE value) {
    bus_write8(cpu->address_space, address, value);
}

static inline void update_zn(cpu_t *cpu, BYTE value) {
    if (value == 0) ZERO_SET; else ZERO_CLEAR;
    if (value & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
}

#ifndef MODE_REGISTER_TYPES_DEFINED
typedef enum {
    INMEDIATE,
    ZEROPAGE,
    ABSOLUTE,
    INDIRECT,
    NOMODE
} mode_t;

typedef enum {
    NONE,
    REGX,
    REGY
} register_t;
#define MODE_REGISTER_TYPES_DEFINED 1
#endif

#define NOTI             fprintf(stderr,"Function %s is not implemented\n",__func__);

static void resolve_address(cpu_t *cpu, mode_t mode, register_t reg, WORD *address, BYTE *value) {
    *address = 0x0000;
    *value = 0x00;
    switch(mode) {
        case INMEDIATE:
            *value = cpu_fetch_byte(cpu, programcounter++);
            break;
        case ZEROPAGE: {
            BYTE zp = cpu_fetch_byte(cpu, programcounter++);
            if (reg == REGX) zp = (BYTE)(zp + regx);
            if (reg == REGY) zp = (BYTE)(zp + regy);
            *address = (WORD)zp;
            break;
        }
        case ABSOLUTE: {
            WORD a = cpu_fetch_word(cpu, programcounter);
            programcounter += 2;
            if (reg == REGX) a = (WORD)(a + regx);
            if (reg == REGY) a = (WORD)(a + regy);
            *address = a;
            break;
        }
        case INDIRECT: {
            if (reg == REGX) {
                BYTE zp = (BYTE)(cpu_fetch_byte(cpu, programcounter++) + regx);
                BYTE lo = cpu_fetch_byte(cpu, (WORD)zp);
                BYTE hi = cpu_fetch_byte(cpu, (WORD)(BYTE)(zp + 1));
                *address = (WORD)((hi << 8) | lo);
            } else if (reg == REGY) {
                BYTE zp = cpu_fetch_byte(cpu, programcounter++);
                BYTE lo = cpu_fetch_byte(cpu, (WORD)zp);
                BYTE hi = cpu_fetch_byte(cpu, (WORD)(BYTE)(zp + 1));
                *address = (WORD)(((hi << 8) | lo) + regy);
            } else {
                WORD ptr = cpu_fetch_word(cpu, programcounter);
                programcounter += 2;
                *address = cpu_fetch_word(cpu, ptr);
            }
            break;
        }
        case NOMODE:
            break;
        default:
            NOTI;
            break;
    }

}

static const char* printMode(mode_t mode) {
    switch(mode) {
    case INMEDIATE: return "INMEDIATE"; break;
    case ZEROPAGE:  return "ZEROPAGE"; break;
    case ABSOLUTE:  return "ABSOLUTE"; break;
    case INDIRECT:  return "INDIRECT"; break;
    case NOMODE:    return "NOMODE"; break;
    default:        return "**error**"; break;
    }
}


void cpu_request_status(cpu_t *cpu) {
    status_requested = true;
}

static void update_status(cpu_t *cpu, mode_t mode, const char *caller) {
    if (!status_requested) return;
    status_requested = false;
    if (cpu->status_callback != NULL)
        cpu->status_callback(cpu, printMode(mode), caller, cpu->status_user_data);
}

#undef STATUS
#define STATUS update_status(cpu, mode, __func__)
                                                                            
static inline bool push_byte(cpu_t *cpu, BYTE v) {
    if (stack_depth >= 256U) return false;
    cpu_store_byte(cpu, (WORD)(0x0100 + stackpointer), v);
    stackpointer--;
    stack_depth++;
    return true;
}

static inline bool pop_byte(cpu_t *cpu, BYTE *out) {
    if (stack_depth == 0U) return false;
    stackpointer++;
    *out = cpu_fetch_byte(cpu, (WORD)(0x0100 + stackpointer));
    stack_depth--;
    return true;
}

#undef PUSH
#define PUSH(a) push_byte(cpu, a)
#undef POP
#define POP(a)  pop_byte(cpu, &(a))

#define IMP NOMODE, NONE
#define IMM INMEDIATE, NONE
#define ZP  ZEROPAGE, NONE
#define ZPX ZEROPAGE, REGX
#define ZPY ZEROPAGE, REGY
#define IZX INDIRECT, REGX
#define IZY INDIRECT, REGY
#define ABS ABSOLUTE, NONE
#define ABX ABSOLUTE, REGX
#define ABY ABSOLUTE, REGY
#define IND INDIRECT, NONE

#undef OPCALL
#define OPCALL(op, addrmode) return op(cpu, addrmode)

static bool wpush(cpu_t *cpu, WORD a ) {
    PUSH((BYTE)((a & 0xFF00)>>8));
    PUSH((BYTE)(a & 0x00FF));
    return true;
}

static bool wpop(cpu_t *cpu, WORD* a ) {
    BYTE lo = {0x00};
    BYTE hi = {0x00};
    POP(lo);
    POP(hi);
    *a = (WORD)((hi<<8)+lo);
    return true;
} 

// Add with carry
static inline bool ADC(cpu_t *cpu, mode_t mode, register_t reg ) {
     update_status(cpu, mode, __func__);
     WORD address = 0x0000;
     BYTE value = 0x00;
     resolve_address(cpu, mode, reg, &address, &value);

     BYTE a = accumulator;
     BYTE addcarry = CARRY_FLAG ? 1 : 0;
     if (mode == INMEDIATE) {
          /* value already set */
     } else {
          value = cpu_fetch_byte(cpu, address);
     }
     uint16_t sum = (uint16_t)a + (uint16_t)value + (uint16_t)addcarry;
     if (sum > 0xFF) CARRY_SET; else CARRY_CLEAR;
     BYTE result = (BYTE)(sum & 0xFF);
     /* overflow: if sign of a and value are same, but sign of result differs */
     if ((~(a ^ value) & (a ^ result) & 0x80) != 0) OVERFLOW_SET; else OVERFLOW_CLEAR;
     accumulator = result;
     if (accumulator == 0) ZERO_SET; else ZERO_CLEAR;
     if (accumulator & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
     return true;
}

// per-opcode base cycle counts (simple table; refine values as needed)
static uint8_t opcode_cycles[256];

static void init_opcode_cycles(void) {
    // default (safe) value
    for (int i = 0; i < 256; i++) opcode_cycles[i] = 2;

    // a few known common base-cycle values (not exhaustive)
    opcode_cycles[0x00] = 7; // BRK
    opcode_cycles[0x20] = 6; // JSR
    opcode_cycles[0x4C] = 3; // JMP ABS
    opcode_cycles[0x6C] = 5; // JMP IND
    opcode_cycles[0x60] = 6; // RTS
    opcode_cycles[0x40] = 6; // RTI
    opcode_cycles[0x69] = 2; // ADC imm
    opcode_cycles[0x65] = 3; // ADC zp
    opcode_cycles[0x6D] = 4; // ADC abs
    opcode_cycles[0x29] = 2; // AND imm
    opcode_cycles[0x2D] = 4; // AND abs
    opcode_cycles[0xA9] = 2; // LDA imm
    opcode_cycles[0xA5] = 3; // LDA zp
    opcode_cycles[0xAD] = 4; // LDA abs
    opcode_cycles[0xEA] = 2; // NOP
    opcode_cycles[0x48] = 3; // PHA
    opcode_cycles[0x68] = 4; // PLA
}

typedef bool (*opcode_fn)(cpu_t *, mode_t, register_t);

typedef struct {
    opcode_fn handler;
    mode_t mode;
    register_t reg;
    uint8_t base_cycles;
} opcode_entry_t;

static opcode_entry_t opcode_table[256];

/* forward declarations for handlers used by the dispatch table */
static inline bool BRK(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool JSR(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool JMP(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool RTS(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool RTI(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool ADC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool AND(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool LDA(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool NOP(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool PHA(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool PLA(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool ASL(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BCC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BCS(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BEQ(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BIT(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BMI(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BNE(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BPL(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BVC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool BVS(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool CLC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool CLD(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool CLI(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool CLV(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool CMP(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool CPX(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool CPY(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool DEC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool DEX(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool DEY(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool EOR(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool INC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool INX(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool INY(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool LDX(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool LDY(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool LSR(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool ORA(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool PHP(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool PLP(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool ROL(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool ROR(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool SBC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool SEC(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool SED(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool SEI(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool STA(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool STX(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool STY(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool TAX(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool TAY(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool TSX(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool TXA(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool TXS(cpu_t *cpu, mode_t mode, register_t reg);
static inline bool TYA(cpu_t *cpu, mode_t mode, register_t reg);

static void init_opcode_table(void) {
    for (int i = 0; i < 256; i++) {
        opcode_table[i].handler = NULL;
        opcode_table[i].mode = NOMODE;
        opcode_table[i].reg = NONE;
        opcode_table[i].base_cycles = opcode_cycles[i];
    }

    /* populate a small set of implemented opcodes */
    opcode_table[0x00] = (opcode_entry_t){BRK, IMP, 7};
    opcode_table[0x20] = (opcode_entry_t){JSR, ABS, 6};
    opcode_table[0x4C] = (opcode_entry_t){JMP, ABS, 3};
    opcode_table[0x6C] = (opcode_entry_t){JMP, IND, 5};
    opcode_table[0x60] = (opcode_entry_t){RTS, IMP, 6};
    opcode_table[0x40] = (opcode_entry_t){RTI, IMP, 6};

    opcode_table[0x69] = (opcode_entry_t){ADC, IMM, 2};
    opcode_table[0x65] = (opcode_entry_t){ADC, ZP, 3};
    opcode_table[0x6D] = (opcode_entry_t){ADC, ABS, 4};

    opcode_table[0x29] = (opcode_entry_t){AND, IMM, 2};
    opcode_table[0x2D] = (opcode_entry_t){AND, ABS, 4};

    opcode_table[0xA9] = (opcode_entry_t){LDA, IMM, 2};
    opcode_table[0xA5] = (opcode_entry_t){LDA, ZP, 3};
    opcode_table[0xAD] = (opcode_entry_t){LDA, ABS, 4};

    opcode_table[0xEA] = (opcode_entry_t){NOP, IMP, 2};
    opcode_table[0x48] = (opcode_entry_t){PHA, IMP, 3};
    opcode_table[0x68] = (opcode_entry_t){PLA, IMP, 4};

    opcode_table[0x01] = (opcode_entry_t){ORA, IZX, 6};
    opcode_table[0x05] = (opcode_entry_t){ORA, ZP, 3};
    opcode_table[0x09] = (opcode_entry_t){ORA, IMM, 2};
    opcode_table[0x0D] = (opcode_entry_t){ORA, ABS, 4};
    opcode_table[0x11] = (opcode_entry_t){ORA, IZY, 5};
    opcode_table[0x15] = (opcode_entry_t){ORA, ZPX, 4};
    opcode_table[0x19] = (opcode_entry_t){ORA, ABY, 4};
    opcode_table[0x1D] = (opcode_entry_t){ORA, ABX, 4};

    opcode_table[0x06] = (opcode_entry_t){ASL, ZP, 5};
    opcode_table[0x0A] = (opcode_entry_t){ASL, IMP, 2};
    opcode_table[0x0E] = (opcode_entry_t){ASL, ABS, 6};
    opcode_table[0x16] = (opcode_entry_t){ASL, ZPX, 6};
    opcode_table[0x1E] = (opcode_entry_t){ASL, ABX, 7};

    opcode_table[0x10] = (opcode_entry_t){BPL, IMM, 2};
    opcode_table[0x30] = (opcode_entry_t){BMI, IMM, 2};
    opcode_table[0x50] = (opcode_entry_t){BVC, IMM, 2};
    opcode_table[0x70] = (opcode_entry_t){BVS, IMM, 2};
    opcode_table[0x90] = (opcode_entry_t){BCC, IMM, 2};
    opcode_table[0xB0] = (opcode_entry_t){BCS, IMM, 2};
    opcode_table[0xD0] = (opcode_entry_t){BNE, IMM, 2};
    opcode_table[0xF0] = (opcode_entry_t){BEQ, IMM, 2};

    opcode_table[0x18] = (opcode_entry_t){CLC, IMP, 2};
    opcode_table[0x38] = (opcode_entry_t){SEC, IMP, 2};
    opcode_table[0x58] = (opcode_entry_t){CLI, IMP, 2};
    opcode_table[0x78] = (opcode_entry_t){SEI, IMP, 2};
    opcode_table[0xB8] = (opcode_entry_t){CLV, IMP, 2};
    opcode_table[0xD8] = (opcode_entry_t){CLD, IMP, 2};
    opcode_table[0xF8] = (opcode_entry_t){SED, IMP, 2};

    opcode_table[0x24] = (opcode_entry_t){BIT, ZP, 3};
    opcode_table[0x2C] = (opcode_entry_t){BIT, ABS, 4};

    opcode_table[0x21] = (opcode_entry_t){AND, IZX, 6};
    opcode_table[0x25] = (opcode_entry_t){AND, ZP, 3};
    opcode_table[0x31] = (opcode_entry_t){AND, IZY, 5};
    opcode_table[0x35] = (opcode_entry_t){AND, ZPX, 4};
    opcode_table[0x39] = (opcode_entry_t){AND, ABY, 4};
    opcode_table[0x3D] = (opcode_entry_t){AND, ABX, 4};

    opcode_table[0x26] = (opcode_entry_t){ROL, ZP, 5};
    opcode_table[0x2A] = (opcode_entry_t){ROL, IMP, 2};
    opcode_table[0x2E] = (opcode_entry_t){ROL, ABS, 6};
    opcode_table[0x36] = (opcode_entry_t){ROL, ZPX, 6};
    opcode_table[0x3E] = (opcode_entry_t){ROL, ABX, 7};

    opcode_table[0x41] = (opcode_entry_t){EOR, IZX, 6};
    opcode_table[0x45] = (opcode_entry_t){EOR, ZP, 3};
    opcode_table[0x49] = (opcode_entry_t){EOR, IMM, 2};
    opcode_table[0x4D] = (opcode_entry_t){EOR, ABS, 4};
    opcode_table[0x51] = (opcode_entry_t){EOR, IZY, 5};
    opcode_table[0x55] = (opcode_entry_t){EOR, ZPX, 4};
    opcode_table[0x59] = (opcode_entry_t){EOR, ABY, 4};
    opcode_table[0x5D] = (opcode_entry_t){EOR, ABX, 4};

    opcode_table[0x46] = (opcode_entry_t){LSR, ZP, 5};
    opcode_table[0x4A] = (opcode_entry_t){LSR, IMP, 2};
    opcode_table[0x4E] = (opcode_entry_t){LSR, ABS, 6};
    opcode_table[0x56] = (opcode_entry_t){LSR, ZPX, 6};
    opcode_table[0x5E] = (opcode_entry_t){LSR, ABX, 7};

    opcode_table[0x61] = (opcode_entry_t){ADC, IZX, 6};
    opcode_table[0x71] = (opcode_entry_t){ADC, IZY, 5};
    opcode_table[0x75] = (opcode_entry_t){ADC, ZPX, 4};
    opcode_table[0x79] = (opcode_entry_t){ADC, ABY, 4};
    opcode_table[0x7D] = (opcode_entry_t){ADC, ABX, 4};

    opcode_table[0x66] = (opcode_entry_t){ROR, ZP, 5};
    opcode_table[0x6A] = (opcode_entry_t){ROR, IMP, 2};
    opcode_table[0x6E] = (opcode_entry_t){ROR, ABS, 6};
    opcode_table[0x76] = (opcode_entry_t){ROR, ZPX, 6};
    opcode_table[0x7E] = (opcode_entry_t){ROR, ABX, 7};

    opcode_table[0x81] = (opcode_entry_t){STA, IZX, 6};
    opcode_table[0x85] = (opcode_entry_t){STA, ZP, 3};
    opcode_table[0x8D] = (opcode_entry_t){STA, ABS, 4};
    opcode_table[0x91] = (opcode_entry_t){STA, IZY, 6};
    opcode_table[0x95] = (opcode_entry_t){STA, ZPX, 4};
    opcode_table[0x99] = (opcode_entry_t){STA, ABY, 5};
    opcode_table[0x9D] = (opcode_entry_t){STA, ABX, 5};

    opcode_table[0x84] = (opcode_entry_t){STY, ZP, 3};
    opcode_table[0x8C] = (opcode_entry_t){STY, ABS, 4};
    opcode_table[0x94] = (opcode_entry_t){STY, ZPX, 4};
    opcode_table[0x86] = (opcode_entry_t){STX, ZP, 3};
    opcode_table[0x8E] = (opcode_entry_t){STX, ABS, 4};
    opcode_table[0x96] = (opcode_entry_t){STX, ZPY, 4};

    opcode_table[0x88] = (opcode_entry_t){DEY, IMP, 2};
    opcode_table[0x8A] = (opcode_entry_t){TXA, IMP, 2};
    opcode_table[0x98] = (opcode_entry_t){TYA, IMP, 2};
    opcode_table[0x9A] = (opcode_entry_t){TXS, IMP, 2};
    opcode_table[0xA8] = (opcode_entry_t){TAY, IMP, 2};
    opcode_table[0xAA] = (opcode_entry_t){TAX, IMP, 2};
    opcode_table[0xBA] = (opcode_entry_t){TSX, IMP, 2};
    opcode_table[0xC8] = (opcode_entry_t){INY, IMP, 2};
    opcode_table[0xCA] = (opcode_entry_t){DEX, IMP, 2};
    opcode_table[0xE8] = (opcode_entry_t){INX, IMP, 2};

    opcode_table[0xA0] = (opcode_entry_t){LDY, IMM, 2};
    opcode_table[0xA4] = (opcode_entry_t){LDY, ZP, 3};
    opcode_table[0xAC] = (opcode_entry_t){LDY, ABS, 4};
    opcode_table[0xB4] = (opcode_entry_t){LDY, ZPX, 4};
    opcode_table[0xBC] = (opcode_entry_t){LDY, ABX, 4};
    opcode_table[0xA1] = (opcode_entry_t){LDA, IZX, 6};
    opcode_table[0xB1] = (opcode_entry_t){LDA, IZY, 5};
    opcode_table[0xB5] = (opcode_entry_t){LDA, ZPX, 4};
    opcode_table[0xB9] = (opcode_entry_t){LDA, ABY, 4};
    opcode_table[0xBD] = (opcode_entry_t){LDA, ABX, 4};
    opcode_table[0xA2] = (opcode_entry_t){LDX, IMM, 2};
    opcode_table[0xA6] = (opcode_entry_t){LDX, ZP, 3};
    opcode_table[0xAE] = (opcode_entry_t){LDX, ABS, 4};
    opcode_table[0xB6] = (opcode_entry_t){LDX, ZPY, 4};
    opcode_table[0xBE] = (opcode_entry_t){LDX, ABY, 4};

    opcode_table[0xC0] = (opcode_entry_t){CPY, IMM, 2};
    opcode_table[0xC4] = (opcode_entry_t){CPY, ZP, 3};
    opcode_table[0xCC] = (opcode_entry_t){CPY, ABS, 4};
    opcode_table[0xC1] = (opcode_entry_t){CMP, IZX, 6};
    opcode_table[0xC5] = (opcode_entry_t){CMP, ZP, 3};
    opcode_table[0xC9] = (opcode_entry_t){CMP, IMM, 2};
    opcode_table[0xCD] = (opcode_entry_t){CMP, ABS, 4};
    opcode_table[0xD1] = (opcode_entry_t){CMP, IZY, 5};
    opcode_table[0xD5] = (opcode_entry_t){CMP, ZPX, 4};
    opcode_table[0xD9] = (opcode_entry_t){CMP, ABY, 4};
    opcode_table[0xDD] = (opcode_entry_t){CMP, ABX, 4};
    opcode_table[0xE0] = (opcode_entry_t){CPX, IMM, 2};
    opcode_table[0xE4] = (opcode_entry_t){CPX, ZP, 3};
    opcode_table[0xEC] = (opcode_entry_t){CPX, ABS, 4};

    opcode_table[0xC6] = (opcode_entry_t){DEC, ZP, 5};
    opcode_table[0xCE] = (opcode_entry_t){DEC, ABS, 6};
    opcode_table[0xD6] = (opcode_entry_t){DEC, ZPX, 6};
    opcode_table[0xDE] = (opcode_entry_t){DEC, ABX, 7};
    opcode_table[0xE6] = (opcode_entry_t){INC, ZP, 5};
    opcode_table[0xEE] = (opcode_entry_t){INC, ABS, 6};
    opcode_table[0xF6] = (opcode_entry_t){INC, ZPX, 6};
    opcode_table[0xFE] = (opcode_entry_t){INC, ABX, 7};

    opcode_table[0xE1] = (opcode_entry_t){SBC, IZX, 6};
    opcode_table[0xE5] = (opcode_entry_t){SBC, ZP, 3};
    opcode_table[0xE9] = (opcode_entry_t){SBC, IMM, 2};
    opcode_table[0xED] = (opcode_entry_t){SBC, ABS, 4};
    opcode_table[0xF1] = (opcode_entry_t){SBC, IZY, 5};
    opcode_table[0xF5] = (opcode_entry_t){SBC, ZPX, 4};
    opcode_table[0xF9] = (opcode_entry_t){SBC, ABY, 4};
    opcode_table[0xFD] = (opcode_entry_t){SBC, ABX, 4};

    opcode_table[0x08] = (opcode_entry_t){PHP, IMP, 3};
    opcode_table[0x28] = (opcode_entry_t){PLP, IMP, 4};
}

// arithmetic shift left
static inline bool ASL(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    if (mode != NOMODE) resolve_address(cpu, mode, reg, &address, &value);
    if (mode == NOMODE) {
        value = accumulator;
        if (value & 0x80) CARRY_SET; else CARRY_CLEAR;
        accumulator = (BYTE)(value << 1);
        update_zn(cpu, accumulator);
    } else {
        value = cpu_fetch_byte(cpu, address);
        if (value & 0x80) CARRY_SET; else CARRY_CLEAR;
        value = (BYTE)(value << 1);
        cpu_store_byte(cpu, address, value);
        update_zn(cpu, value);
    }
    return true;
}
// and (with accumulator)
static inline bool AND(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode == INMEDIATE) {
        /* value already set */
    } else {
        value = cpu_fetch_byte(cpu, address);
    }
    accumulator &= value;
    if (accumulator == 0) ZERO_SET; else ZERO_CLEAR;
    if (accumulator & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return true;
}

// branch on carry clear
static inline bool BCC(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && !CARRY_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// branch on carry set
static inline bool BCS(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && CARRY_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// branch on equal (zero set)
static inline bool BEQ(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && ZERO_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// bit test
static inline bool BIT(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    value = cpu_fetch_byte(cpu, address);
    if ((accumulator & value) == 0) ZERO_SET; else ZERO_CLEAR;
    if (value & 0x40) OVERFLOW_SET; else OVERFLOW_CLEAR;
    if (value & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return true;
}

// branch on minus (negative set)
static inline bool BMI(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && NEGATIVE_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// branch on not equal (zero clear)
static inline bool BNE(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && !ZERO_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// branch on plus (negative clear)
static inline bool BPL(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && !NEGATIVE_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// break / interrupt
static inline bool BRK(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    programcounter++;
    wpush(cpu, programcounter);
    PUSH((BYTE)(procstatus | (1U << BREAK_BIT) | 0x20U));
    BREAK_SET;
    INTERRUPT_SET;
    programcounter = cpu_fetch_word(cpu, 0xFFFE);
    return true;
}

// branch on overflow clear
static inline bool BVC(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && !OVERFLOW_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// branch on overflow set
static inline bool BVS(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if ((mode == INMEDIATE) && OVERFLOW_FLAG) {
        programcounter = (WORD)(programcounter + (int8_t)value);
    }
    return true;
}

// clear carry
static inline bool CLC(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    CARRY_CLEAR;
    return true;
}

// clear decimal
static inline bool CLD(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    DECIMAL_CLEAR;
    return true;
}

// clear interrupt disable
static inline bool CLI(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    INTERRUPT_CLEAR;
    return true;
}

// clear overflow
static inline bool CLV(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    OVERFLOW_CLEAR;
    return true;
}

// compare (with accumulator)
static inline bool CMP(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    BYTE result = (BYTE)(accumulator - value);
    if (accumulator >= value) CARRY_SET; else CARRY_CLEAR;
    if (result == 0) ZERO_SET; else ZERO_CLEAR;
    if ((result & 0x80) != 0) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return true;
}

// compare with X
static inline bool CPX(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    BYTE result = (BYTE)(regx - value);
    if (regx >= value) CARRY_SET; else CARRY_CLEAR;
    if (result == 0) ZERO_SET; else ZERO_CLEAR;
    if ((result & 0x80) != 0) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return true;
}


// compare with Y
static inline bool CPY(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    BYTE result = (BYTE)(regy - value);
    if (regy >= value) CARRY_SET; else CARRY_CLEAR;
    if (result == 0) ZERO_SET; else ZERO_CLEAR;
    if ((result & 0x80) != 0) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return true;
}

// decrement
static inline bool DEC(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) {
        value = cpu_fetch_byte(cpu, address);
        value = (BYTE)(value - 1);
        cpu_store_byte(cpu, address, value);
        if (value == 0) ZERO_SET; else ZERO_CLEAR;
        if (value & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    }
    return true;
}

// decrement X
static inline bool DEX(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    regx--;
    update_zn(cpu, regx);
    return true;
}

// decrement Y
static inline bool DEY(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    regy--;
    update_zn(cpu, regy);
    return true;
}

// exclusive or (with accumulator)
static inline bool EOR(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    bool ok = true;
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    accumulator ^= value;
    if (accumulator == 0) ZERO_SET; else ZERO_CLEAR;
    if (accumulator & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return ok;
}

// increment
static inline bool INC(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) {
        value = cpu_fetch_byte(cpu, address);
        value = (BYTE)(value + 1);
        cpu_store_byte(cpu, address, value);
        if (value == 0) ZERO_SET; else ZERO_CLEAR;
        if (value & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    }
    return true;
}

// increment X
static inline bool INX(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    regx++;
    update_zn(cpu, regx);
    return true;
}

// increment Y
static inline bool INY(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    regy++;
    update_zn(cpu, regy);
    return true;
}

// jump
static inline bool JMP(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    programcounter = address;
    return true;
}

// jump subroutine
static inline bool JSR(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    wpush(cpu, (WORD)(programcounter - 1));
    programcounter = address;
    return true;
}

// load accumulator
static inline bool LDA(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode == INMEDIATE) {
        /* value already set */
    } else {
        value = cpu_fetch_byte(cpu, address);
    }
    accumulator = value;
    if (accumulator == 0) ZERO_SET; else ZERO_CLEAR;
    if (accumulator & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return true;
}

// load X
static inline bool LDX(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    bool ok = true;
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    regx = value;
    if (regx == 0) ZERO_SET; else ZERO_CLEAR;
    if (regx & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return ok;
}

// load Y
static inline bool LDY(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    bool ok = true;
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    regy = value;
    if (regy == 0) ZERO_SET; else ZERO_CLEAR;
    if (regy & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return ok;
}

// logical shift right
static inline bool LSR(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    if (mode != NOMODE) resolve_address(cpu, mode, reg, &address, &value);
    if (mode == NOMODE) {
        value = accumulator;
        if (value & 0x01) CARRY_SET; else CARRY_CLEAR;
        accumulator = (BYTE)(value >> 1);
        update_zn(cpu, accumulator);
    } else {
        value = cpu_fetch_byte(cpu, address);
        if (value & 0x01) CARRY_SET; else CARRY_CLEAR;
        value = (BYTE)(value >> 1);
        cpu_store_byte(cpu, address, value);
        update_zn(cpu, value);
    }
    return true;
}

//  no operation
static inline bool NOP(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    return true;
}

// or with accumulator
static inline bool ORA(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    bool ok = true;
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    accumulator |= value;
    if (accumulator == 0) ZERO_SET; else ZERO_CLEAR;
    if (accumulator & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return ok;
}

// push accumulator
static inline bool PHA(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    PUSH(accumulator);
    return true;
} 

// push processor status (SR)
static inline bool PHP(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    PUSH((BYTE)(procstatus | (1U << BREAK_BIT) | 0x20U));
    return true;
}

// pull accumulator
static inline bool PLA(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    POP(accumulator);
    if (accumulator == 0) ZERO_SET; else ZERO_CLEAR;
    if (accumulator & 0x80) NEGATIVE_SET; else NEGATIVE_CLEAR;
    return true;
}

// pull processor status (SR)
static inline bool PLP(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    POP(procstatus);
    return true;
}

// rotate left
static inline bool ROL(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    if (mode != NOMODE) resolve_address(cpu, mode, reg, &address, &value);
    if (mode == NOMODE) {
        value = accumulator;
        BYTE carry_in = CARRY_FLAG ? 1 : 0;
        if (value & 0x80) CARRY_SET; else CARRY_CLEAR;
        accumulator = (BYTE)((value << 1) | carry_in);
        update_zn(cpu, accumulator);
    } else {
        value = cpu_fetch_byte(cpu, address);
        BYTE carry_in = CARRY_FLAG ? 1 : 0;
        if (value & 0x80) CARRY_SET; else CARRY_CLEAR;
        value = (BYTE)((value << 1) | carry_in);
        cpu_store_byte(cpu, address, value);
        update_zn(cpu, value);
    }
    return true;
}

// rotate right
static inline bool ROR(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    if (mode != NOMODE) resolve_address(cpu, mode, reg, &address, &value);
    if (mode == NOMODE) {
        value = accumulator;
        BYTE carry_in = CARRY_FLAG ? 0x80 : 0x00;
        if (value & 0x01) CARRY_SET; else CARRY_CLEAR;
        accumulator = (BYTE)((value >> 1) | carry_in);
        update_zn(cpu, accumulator);
    } else {
        value = cpu_fetch_byte(cpu, address);
        BYTE carry_in = CARRY_FLAG ? 0x80 : 0x00;
        if (value & 0x01) CARRY_SET; else CARRY_CLEAR;
        value = (BYTE)((value >> 1) | carry_in);
        cpu_store_byte(cpu, address, value);
        update_zn(cpu, value);
    }
    return true;
}

// return from interrupt
static inline bool RTI(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    /* simple RTI: pop status then PC */
    POP(procstatus);
    wpop(cpu, &programcounter);
    return true;
}

// return from subroutine
static inline bool RTS(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    wpop(cpu, &programcounter);
    programcounter++;
    return true;
}

// subtract with carry
static inline bool SBC(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    if (mode != INMEDIATE) value = cpu_fetch_byte(cpu, address);
    BYTE a = accumulator;
    uint16_t borrow = CARRY_FLAG ? 0U : 1U;
    uint16_t diff = (uint16_t)a - (uint16_t)value - borrow;
    BYTE result = (BYTE)(diff & 0xFF);
    if (diff < 0x100) CARRY_SET; else CARRY_CLEAR;
    if (((a ^ result) & (a ^ value) & 0x80) != 0) OVERFLOW_SET; else OVERFLOW_CLEAR;
    accumulator = result;
    update_zn(cpu, accumulator);
    return true;
}

// set carry
static inline bool SEC(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    CARRY_SET;
    return true;
}

// set decimal
static inline bool SED(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    DECIMAL_SET;
    return true;
}

// set interrupt disable
static inline bool SEI(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    INTERRUPT_SET;
    return true;
}

// store accumulator
static inline bool STA(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    bool ok = true;
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    cpu_store_byte(cpu, address, accumulator);
    return ok;
}

// store X
static inline bool STX(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    bool ok = true;
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    cpu_store_byte(cpu, address, regx);
    return ok;
}

// store Y
static inline bool STY(cpu_t *cpu, mode_t mode, register_t reg ) {
    update_status(cpu, mode, __func__);
    bool ok = true;
    WORD address = 0x0000;
    BYTE value = 0x00;
    resolve_address(cpu, mode, reg, &address, &value);
    cpu_store_byte(cpu, address, regy);
    return ok;
}

// transfer accumulator to X
static inline bool TAX(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    regx = accumulator;
    update_zn(cpu, regx);
    return true;
}

// transfer accumulator to Y
static inline bool TAY(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    regy = accumulator;
    update_zn(cpu, regy);
    return true;
}

// transfer stack pointer to X
static inline bool TSX(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    regx = stackpointer;
    update_zn(cpu, regx);
    return true;
}

// transfer X to accumulator
static inline bool TXA(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    accumulator = regx;
    update_zn(cpu, accumulator);
    return true;
}

// transfer Y to stack pointer
static inline bool TXS(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    stackpointer = regx;
    return true;
}

// transfer Y to accumulator
static inline bool TYA(cpu_t *cpu, mode_t mode, register_t reg ) {
    (void)reg;
    update_status(cpu, mode, __func__);
    accumulator = regy;
    update_zn(cpu, accumulator);
    return true;
}



/*------------------------------------------------------------------------------------------------------------------------------
 * HI	LO-NIBBLE
 *      ‐0	        ‐1	        ‐2	‐3	‐4	‐5	    ‐6	    ‐7	‐8	        ‐9	    ‐A	    ‐B	‐C	‐D	    ‐E	    ‐F
 * 0‐	BRK impl	ORA X,ind	---	---	---	ORA zpg	ASL zpg	---	PHP impl	ORA #	ASL A	---	---	ORA abs	ASL abs	---
 * 1‐	BPL rel	ORA ind,Y	---	---	---	ORA zpg,X	ASL zpg,X	---	CLC impl	ORA abs,Y	---	---	---	ORA abs,X	ASL abs,X	---
 * 2‐	JSR abs	AND X,ind	---	---	BIT zpg	AND zpg	ROL zpg	---	PLP impl	AND #	ROL A	---	BIT abs	AND abs	ROL abs	---
 * 3‐	BMI rel	AND ind,Y	---	---	---	AND zpg,X	ROL zpg,X	---	SEC impl	AND abs,Y	---	---	---	AND abs,X	ROL abs,X	---
 * 4‐	RTI impl	EOR X,ind	---	---	---	EOR zpg	LSR zpg	---	PHA impl	EOR #	LSR A	---	JMP abs	EOR abs	LSR abs	---
 * 5‐	BVC rel	EOR ind,Y	---	---	---	EOR zpg,X	LSR zpg,X	---	CLI impl	EOR abs,Y	---	---	---	EOR abs,X	LSR abs,X	---
 * 6‐	RTS impl	ADC X,ind	---	---	---	ADC zpg	ROR zpg	---	PLA impl	ADC #	ROR A	---	JMP ind	ADC abs	ROR abs	---
 * 7‐	BVS rel	ADC ind,Y	---	---	---	ADC zpg,X	ROR zpg,X	---	SEI impl	ADC abs,Y	---	---	---	ADC abs,X	ROR abs,X	---
 * 8‐	---	STA X,ind	---	---	STY zpg	STA zpg	STX zpg	---	DEY impl	---	TXA impl	---	STY abs	STA abs	STX abs	---
 * 9‐	BCC rel	STA ind,Y	---	---	STY zpg,X	STA zpg,X	STX zpg,Y	---	TYA impl	STA abs,Y	TXS impl	---	---	STA abs,X	---	---
 * A‐	LDY #	LDA X,ind	LDX #	---	LDY zpg	LDA zpg	LDX zpg	---	TAY impl	LDA #	TAX impl	---	LDY abs	LDA abs	LDX abs	---
 * B‐	BCS rel	LDA ind,Y	---	---	LDY zpg,X	LDA zpg,X	LDX zpg,Y	---	CLV impl	LDA abs,Y	TSX impl	---	LDY abs,X	LDA abs,X	LDX abs,Y	---
 * C‐	CPY #	CMP X,ind	---	---	CPY zpg	CMP zpg	DEC zpg	---	INY impl	CMP #	DEX impl	---	CPY abs	CMP abs	DEC abs	---
 * D‐	BNE rel	CMP ind,Y	---	---	---	CMP zpg,X	DEC zpg,X	---	CLD impl	CMP abs,Y	---	---	---	CMP abs,X	DEC abs,X	---
 * E‐	CPX #	SBC X,ind	---	---	CPX zpg	SBC zpg	INC zpg	---	INX impl	SBC #	NOP impl	---	CPX abs	SBC abs	INC abs	---
 * F‐	BEQ rel	SBC ind,Y	---	---	---	SBC zpg,X	INC zpg,X	---	SED impl	SBC abs,Y	---	---	---	SBC abs,X	INC abs,X	---
 */
BYTE cpu_fetch(cpu_t *cpu) {
    return cpu_fetch_byte(cpu, programcounter++);
}
bool cpu_execute(cpu_t *cpu, BYTE instruction) {
    bool ok = true;

    current_instruction = instruction;

    /* fast-path: if we've registered a table handler for this opcode, use it */
    opcode_entry_t entry = opcode_table[(uint8_t)instruction];
    if (entry.handler != NULL) {
        // Account base cycles; refine with addressing/page penalties later.
        cycles += entry.base_cycles;
        return entry.handler(cpu, entry.mode, entry.reg);
    }

    cycles += opcode_cycles[(uint8_t)instruction];

    switch(instruction) {
        case 0x00: OPCALL(BRK,IMP);
        case 0x01: OPCALL(ORA,IZX);
        case 0x02: break; //reserved
        case 0x03: break; //reserved
        case 0x04: break; //reserved 
        case 0x05: OPCALL(ORA,ZP);
        case 0x06: OPCALL(ASL,ZP);
        case 0x07: break;
        case 0x08: OPCALL(PHP,IMP);
        case 0x09: OPCALL(ORA,IMM);
        case 0x0A: OPCALL(ASL,IMP);
        case 0x0B: break;
        case 0x0C: break;
        case 0x0D: break;
        case 0x0E: break;
        case 0x0F: break;
        case 0x10: OPCALL(BPL,IMM);
        case 0x11: OPCALL(ORA,IZY); 
        case 0x12: break;
        case 0x13: break;
        case 0x14: break;
        case 0x15: OPCALL(ORA,ZPX);
        case 0x16: break;
        case 0x17: break;
        case 0x18: OPCALL(CLC,IMM);
        case 0x19: break;
        case 0x1A: break;
        case 0x1B: break;
        case 0x1C: break;
        case 0x1D: break;
        case 0x1E: break;
        case 0x1F: break;
        case 0x20: OPCALL(JSR,ABS);
        case 0x21: OPCALL(AND,IZX);
        case 0x22: break;
        case 0x23: break;
        case 0x24: OPCALL(BIT,ZP);
        case 0x25: OPCALL(AND,ZP);
        case 0x26: OPCALL(ROL,ZP);
        case 0x27: break;
        case 0x28: OPCALL(PLP,IMP);
        case 0x29: OPCALL(AND,IMM);
        case 0x2A: OPCALL(ROL,IMP);
        case 0x2B: break;
        case 0x2C: OPCALL(BIT,ABS);
        case 0x2D: OPCALL(AND,ABS);
        case 0x2E: OPCALL(ROL,ABS);
        case 0x2F: break;
        case 0x30: OPCALL(BMI,IMM);
        case 0x31: OPCALL(AND,IZY);
        case 0x32: break;
        case 0x33: break;
        case 0x34: break;
        case 0x35: OPCALL(AND,ZPX);
        case 0x36: OPCALL(ROL,ZPX);
        case 0x37: break;
        case 0x38: OPCALL(SEC,ABS);
        case 0x39: OPCALL(AND,ABY);
        case 0x3A: break;
        case 0x3B: break;
        case 0x3C: break;
        case 0x3D: OPCALL(AND,ABX);
        case 0x3E: OPCALL(ROL,ABX);
        case 0x3F: break;
        case 0x40: OPCALL(RTI,IMP);
        case 0x41: OPCALL(EOR,IZX);
        case 0x42: break;
        case 0x43: break;
        case 0x44: break;
        case 0x45: OPCALL(EOR,ZP);
        case 0x46: OPCALL(LSR,ZP);
        case 0x47: break;
        case 0x48: OPCALL(PHA,IMP);
        case 0x49: OPCALL(EOR,IMM); 
        case 0x4A: OPCALL(LSR,IMP);
        case 0x4B: break;
        case 0x4C: OPCALL(JMP,ABS);
        case 0x4D: OPCALL(EOR,ABS); 
        case 0x4E: OPCALL(LSR,ABS);
        case 0x4F: break;
        case 0x50: OPCALL(BVC,IMM);
        case 0x51: OPCALL(EOR,IZY);
        case 0x52: break;
        case 0x53: break;
        case 0x54: break;
        case 0x55: OPCALL(EOR,ZPX);
        case 0x56: OPCALL(LSR,ZPX);
        case 0x57: break;
        case 0x58: OPCALL(CLI,IMP);
        case 0x59: OPCALL(EOR,ABY);
        case 0x5A: break;
        case 0x5B: break;
        case 0x5C: break;
        case 0x5D: OPCALL(EOR,ABX);
        case 0x5E: OPCALL(LSR,ABX);
        case 0x5F: break;
        case 0x60: OPCALL(RTS,IMP);
        case 0x61: OPCALL(ADC,IZX);
        case 0x62: break;
        case 0x63: break;
        case 0x64: break;
        case 0x65: OPCALL(ADC,ZP);
        case 0x66: OPCALL(ROR,ZP);
        case 0x67: break;
        case 0x68: break;
        case 0x69: OPCALL(ADC,IMM);
        case 0x6A: OPCALL(ROR,IMP);
        case 0x6B: OPCALL(PLA,IMP);
        case 0x6C: OPCALL(JMP,IND);
        case 0x6D: OPCALL(ADC,ABS);
        case 0x6E: OPCALL(ROR,ABS);
        case 0x6F: break;
        case 0x70: OPCALL(BVS,IMM);
        case 0x71: OPCALL(ADC,IZY);
        case 0x72: break;
        case 0x73: break;
        case 0x74: break;
        case 0x75: OPCALL(ADC,ZPX);
        case 0x76: OPCALL(ROR,ZPX);
        case 0x77: break;
        case 0x78: OPCALL(SEI,IMP);
        case 0x79: OPCALL(ADC,ABY);
        case 0x7A: break;
        case 0x7B: break;
        case 0x7C: break;
        case 0x7D: OPCALL(ADC,ABX);
        case 0x7E: OPCALL(ROR,ABX);
        case 0x7F: break;
        case 0x80: break;
        case 0x81: OPCALL(STA,IZX);
        case 0x82: break;
        case 0x83: break;
        case 0x84: OPCALL(STY,ZP);
        case 0x85: OPCALL(STA,ZP);
        case 0x86: OPCALL(STX,ZP);
        case 0x87: break;
        case 0x88: OPCALL(DEY,IMP);
        case 0x89: break;
        case 0x8A: OPCALL(TXA,IMP);
        case 0x8B: break;
        case 0x8C: OPCALL(STY,ABS);
        case 0x8D: break;
        case 0x8E: OPCALL(STX,ABS);
        case 0x8F: break;
        case 0x90: OPCALL(BCC,IMM);
        case 0x91: OPCALL(STA,IZY);
        case 0x92: break;
        case 0x93: break;
        case 0x94: OPCALL(STY,ZPX);
        case 0x95: OPCALL(STA,ZPX);
        case 0x96: OPCALL(STX,ZPY);
        case 0x97: break;
        case 0x98: OPCALL(TYA,IMP);
        case 0x99: OPCALL(STA,ABY);
        case 0x9A: OPCALL(TXS,IMP);
        case 0x9B: break;
        case 0x9C: break;
        case 0x9D: break;
        case 0x9E: break;
        case 0x9F: break;
        case 0xA0: OPCALL(LDY,IMM);
        case 0xA1: OPCALL(LDA,IZX);
        case 0xA2: OPCALL(LDX,IMM);
        case 0xA3: break;
        case 0xA4: OPCALL(LDY, ZP);
        case 0xA5: OPCALL(LDA, ZP);
        case 0xA6: OPCALL(LDX, ZP);
        case 0xA7: break;
        case 0xA8: break;
        case 0xA9: OPCALL(LDA,IMM);
        case 0xAA: OPCALL(TAX, IMP);
        case 0xAB: OPCALL(TAY, IMP);
        case 0xAC: OPCALL(LDY,ABS);
        case 0xAD: OPCALL(LDA,ABS);
        case 0xAE: OPCALL(LDX,ABS);
        case 0xAF: break;
        case 0xB0: OPCALL(BCS,IMM);
        case 0xB1: OPCALL(LDA,IZY); 
        case 0xB2: break;
        case 0xB3: break;
        case 0xB4: OPCALL(LDY, ZPX);
        case 0xB5: OPCALL(LDA, ZPX);
        case 0xB6: OPCALL(LDX, ZPY);
        case 0xB7: break;
        case 0xB8: OPCALL(CLV, IMP);
        case 0xB9: OPCALL(LDA,ABY);
        case 0xBA: OPCALL(TSX, IMP);
        case 0xBB: break;
        case 0xBC: OPCALL(LDY,ABX);
        case 0xBD: OPCALL(LDA,ABX);
        case 0xBE: OPCALL(LDX,ABY);
        case 0xBF: break;
        case 0xC0: OPCALL(CPY,IMM);
        case 0xC1: OPCALL(CMP,IZX);
        case 0xC2: break;
        case 0xC3: break;
        case 0xC4: OPCALL(CPY,ZP);
        case 0xC5: OPCALL(CMP,ZP);
        case 0xC6: OPCALL(DEC,ZP);
        case 0xC7: break;
        case 0xC8: OPCALL(INY,IMP);
        case 0xC9: OPCALL(CMP,IMM);
        case 0xCA: OPCALL(DEX,IMP);
        case 0xCB: break;
        case 0xCC: OPCALL(CPY,ABS);
        case 0xCD: OPCALL(CMP,ABS);
        case 0xCE: OPCALL(DEC,ABS);
        case 0xCF: break;
        case 0xD0: OPCALL(BNE,IMM);
        case 0xD1: OPCALL(CMP,IZY);
        case 0xD2: break;
        case 0xD3: break;
        case 0xD4: break;
        case 0xD5: OPCALL(CMP,ZPX);
        case 0xD6: OPCALL(DEC,ZPX);
        case 0xD7: break;
        case 0xD8: OPCALL(CLD,IMM);
        case 0xD9: OPCALL(CMP,ABY);
        case 0xDA: break;
        case 0xDB: break;
        case 0xDC: break;
        case 0xDD: OPCALL(CMP,ABX);
        case 0xDE: OPCALL(DEC,ABX);
        case 0xDF: break;
        case 0xE0: OPCALL(CPX,IMM);
        case 0xE1: OPCALL(SBC,IZX);
        case 0xE2: break;
        case 0xE3: break;
        case 0xE4: OPCALL(CPX,ZP);
        case 0xE5: OPCALL(SBC,ZP);
        case 0xE6: OPCALL(INC,ZP);
        case 0xE7: break;
        case 0xE8: OPCALL(INX,IMP);
        case 0xE9: OPCALL(SBC,IMM);
        case 0xEA: OPCALL(NOP,IMP);
        case 0xEB: break;
        case 0xEC: OPCALL(CPX,ABS);
        case 0xED: OPCALL(SBC,ABS);
        case 0xEE: OPCALL(INC,ABS);
        case 0xEF: break;
        case 0xF0: OPCALL(BEQ,IMM);
        case 0xF1: OPCALL(SBC,IZY);
        case 0xF2: break;
        case 0xF3: break;
        case 0xF4: break;
        case 0xF5: OPCALL(SBC,ZPX);
        case 0xF6: OPCALL(INC,ZPX);
        case 0xF7: break;
        case 0xF8: OPCALL(SED,IMM);
        case 0xF9: OPCALL(SBC,ABY);
        case 0xFA: break;
        case 0xFB: break;
        case 0xFC: break;
        case 0xFD: OPCALL(SBC,ABX);
        case 0xFE: OPCALL(INC,ABX);
        case 0xFF: break;
    }
    return ok;
}

void cpu_reset(cpu_t *cpu) {
    status_requested = false;
    stackpointer = 0xFF; /* 6502 SP starts at 0xFF and grows down */
    stack_depth = 0U;
    accumulator = 0x00;
    regx = 0x00;
    regy = 0x00;
    procstatus = 0x20;
    current_instruction = 0x00;
    programcounter = cpu_fetch_word(cpu, 0xFFFC);
    cycles = 0ULL;
}

bool cpu_init(cpu_t *cpu, address_space_t *address_space) {
    if (cpu == NULL || !address_space_valid(address_space)) return false;
    if (!initialised) {
        init_opcode_cycles();
        init_opcode_table();
        initialised = true;
    }
    *cpu = (cpu_t){0};
    cpu->address_space = address_space;
    cpu_reset(cpu);
    return true;
}

unsigned long long cpu_get_cycles(const cpu_t *cpu) {
    return cycles;
}

bool cpu_tick(cpu_t *cpu) {
    return cpu_execute(cpu, cpu_fetch(cpu));
}

bool cpu_test(void) {
    BYTE test_memory[0x10000] = {0};
    cpu_t test_cpu;
    cpu_t *cpu = &test_cpu;
    bool ok = true;
    BYTE val = 0x00;

    physical_memory_t physical;
    address_space_t space;
    if (!memory_init(&physical, test_memory, sizeof test_memory) ||
        !address_space_init(&space, &physical) || !cpu_init(cpu, &space)) {
        printf("Initialisation failed\n");
        return false;
    }

    /* push 0..255 onto stack */
    for (int i = 0; i < 256; i++) {
        if (!PUSH((BYTE)i)) {
            fprintf(stderr, "cpu_test: PUSH failed at %d\n", i);
            return false;
        }
    }

    /* pop and verify LIFO order */
    for (int i = 255; i >= 0; i--) {
        if (!POP(val)) {
            fprintf(stderr, "cpu_test: POP failed at %d\n", i);
            return false;
        }
        if (val != (BYTE)i) {
            fprintf(stderr, "cpu_test: stack value mismatch expected %d got %d\n", i, val);
            return false;
        }
    }

    /* processor status flag basics */
    procstatus = 0x00;
    ok = ok && (!CARRY_FLAG);
    ok = ok && (!ZERO_FLAG);
    ok = ok && (!INTERRUPT_FLAG);
    ok = ok && (!DECIMAL_FLAG);
    ok = ok && (!BREAK_FLAG);
    ok = ok && (!OVERFLOW_FLAG);
    ok = ok && (!NEGATIVE_FLAG);
    if (!ok) return ok;

    procstatus = 0xFF;
    ok = ok && (CARRY_FLAG);
    ok = ok && (ZERO_FLAG);
    ok = ok && (INTERRUPT_FLAG);
    ok = ok && (DECIMAL_FLAG);
    ok = ok && (BREAK_FLAG);
    ok = ok && (OVERFLOW_FLAG);
    ok = ok && (NEGATIVE_FLAG);
    if (!ok) return ok;

    procstatus = 0x00;
    CARRY_SET;
    ok = ok && (CARRY_FLAG);
    CARRY_CLEAR;
    ok = ok && (!CARRY_FLAG);
    if (!ok) return ok;

    /* test a couple of opcode executions that affect flags */
    ok = ok && cpu_execute(cpu, 0xF8);  // SED
    ok = ok && (DECIMAL_FLAG);
    ok = ok && cpu_execute(cpu, 0xD8);  // CLD
    ok = ok && (!DECIMAL_FLAG);

    return ok;
}


