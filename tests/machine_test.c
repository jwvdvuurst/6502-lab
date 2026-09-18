#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "6502machine.h"
#include "6502framebuffer.h"
static void same_cpu(const cpu_t *a,const cpu_t *b) {
    assert(a->A==b->A && a->X==b->X && a->Y==b->Y && a->SP==b->SP && a->P==b->P);
    assert(a->PC==b->PC && a->cycles==b->cycles && a->current_instruction==b->current_instruction);
    assert(a->stack_depth==b->stack_depth && a->status_requested==b->status_requested);
    assert(a->address_space==b->address_space && a->status_callback==b->status_callback);
    assert(a->status_user_data==b->status_user_data);
}
static void count_write(const physical_memory_t *memory,WORD block,WORD offset,
                        BYTE before,BYTE after,void *data) {
    assert(memory_read8(memory,block,offset)==before);
    (void)after; (*(unsigned *)data)++;
}
static void test_roundtrip(void) {
    BYTE bytes[65*1024]={0}, initial[65*1024], final[65*1024];
    physical_memory_t memory; address_space_t space; cpu_t cpu;
    assert(memory_init(&memory,bytes,sizeof bytes) && address_space_init(&space,&memory));
    assert(address_space_map(&space,0,64));
    const BYTE code[]={0xA9,0x2A,0x85,0x10,0x20,0x10,0x08,0x8D,0x00,0xC0,
                       0x4C,0x00,0x08,0xEA,0xEA,0xEA,0x48,0xE8,0x68,0x60};
    for(size_t i=0;i<sizeof code;i++) bus_write8(&space,(WORD)(0x800+i),code[i]);
    bus_write16(&space,0xFFFC,0x0800); assert(cpu_init(&cpu,&space));
    cpu_request_status(&cpu);
    cpu_t original=cpu; address_space_t original_space=space;
    memcpy(initial,bytes,sizeof bytes);
    unsigned writes=0; memory.write_observer=count_write; memory.observer_data=&writes;
    machine_t *m=machine_create(&cpu,256); assert(m);
    assert(!machine_create(&cpu,4));
    for(int i=0;i<200;i++) assert(machine_step(m)==MACHINE_OK);
    cpu_t final_cpu=cpu; memcpy(final,bytes,sizeof bytes);
    unsigned forward_writes=writes; assert(forward_writes>0 && bytes[0xC000]==0x2A);
    for(int i=0;i<200;i++) assert(machine_step_back(m)==MACHINE_OK);
    same_cpu(&cpu,&original); assert(memcmp(bytes,initial,sizeof bytes)==0);
    assert(memcmp(space.blocks,original_space.blocks,sizeof space.blocks)==0);
    assert(writes==forward_writes && machine_position(m)==0 && machine_newest(m)==200);
    for(int i=0;i<200;i++) assert(machine_step(m)==MACHINE_OK);
    same_cpu(&cpu,&final_cpu); assert(memcmp(bytes,final,sizeof bytes)==0);
    assert(writes==2*forward_writes);
    machine_destroy(m);
    assert(memory.write_guard==NULL && memory.write_observer==count_write);
    assert(bus_write8(&space,0x10,0x99));
}
static bool repeated_mapped_writes(cpu_t *cpu,void *data) {
    (void)data;
    assert(address_space_map(cpu->address_space,0,65));
    bus_write8(cpu->address_space,0x10,0xAA);
    bus_write8(cpu->address_space,0x10,0xBB);
    assert(address_space_map(cpu->address_space,0,64));
    bus_write8(cpu->address_space,0x10,0xCC);
    cpu->X=0xFF;
    return true;
}
static bool failed_action(cpu_t *cpu,void *data) {
    (void)data; cpu->PC=0xFFFF; cpu->P=0;
    bus_write8(cpu->address_space,0x10,0xDD);
    assert(address_space_map(cpu->address_space,0,65));
    bus_write8(cpu->address_space,0x10,0xEE);
    return false;
}
static bool too_many_writes(cpu_t *cpu,void *data) {
    (void)data; cpu->A=0xFF;
    for(unsigned i=0;i<MACHINE_MAX_WRITES+1;i++) bus_write8(cpu->address_space,(WORD)i,(BYTE)(i+1));
    return true; /* Even an action ignoring the veto must be rolled back. */
}
static bool recursive_action(cpu_t *cpu,void *data) {
    (void)cpu;
    assert(machine_step(data)==MACHINE_INVALID && machine_step_back(data)==MACHINE_INVALID);
    return false;
}
static void test_transactions(void) {
    BYTE bytes[66*1024]={0}, saved[66*1024];
    physical_memory_t memory; address_space_t space; cpu_t cpu;
    assert(memory_init(&memory,bytes,sizeof bytes) && address_space_init(&space,&memory));
    assert(address_space_map(&space,0,64)); bus_write8(&space,0x10,0x11);
    memory_write8(&memory,65,0x10,0x22); assert(cpu_init(&cpu,&space));
    assert(!machine_create(&cpu,0) && !machine_create(&cpu,MACHINE_MAX_HISTORY+1));
    assert(!machine_create(NULL,2));
    machine_t *m=machine_create(&cpu,8); assert(m);
    assert(!bus_write8(&space,0x10,0xFF) && bus_read8(&space,0x10)==0x11);
    framebuffer_t fb; assert(framebuffer_init(&fb,&memory,48));
    assert(!framebuffer_clear(&fb) && bytes[0xC000]==0);
    memcpy(saved,bytes,sizeof bytes); cpu_t original=cpu;
    assert(machine_transaction(m,repeated_mapped_writes,NULL)==MACHINE_OK);
    assert(bytes[65*1024+0x10]==0xBB && bytes[64*1024+0x10]==0xCC);
    assert(machine_step_back(m)==MACHINE_OK && machine_position(m)==0 && machine_newest(m)==1);
    assert(memcmp(bytes,saved,sizeof bytes)==0 && space.blocks[0]==64); same_cpu(&cpu,&original);
    assert(machine_transaction(m,failed_action,NULL)==MACHINE_ACTION_FAILED);
    assert(memcmp(bytes,saved,sizeof bytes)==0 && space.blocks[0]==64); same_cpu(&cpu,&original);
    assert(machine_position(m)==0 && machine_newest(m)==1);
    assert(machine_transaction(m,too_many_writes,NULL)==MACHINE_WRITE_LIMIT);
    assert(memcmp(bytes,saved,sizeof bytes)==0); same_cpu(&cpu,&original);
    assert(machine_newest(m)==1 && machine_position(m)==0);
    assert(machine_transaction(m,recursive_action,m)==MACHINE_ACTION_FAILED);
    assert(machine_transaction(m,NULL,NULL)==MACHINE_INVALID);
    assert(machine_poke(m,0x10,0x33)==MACHINE_OK && machine_newest(m)==1);
    assert(machine_poke(m,0x10,0x44)==MACHINE_OK);
    assert(machine_poke(m,0x10,0x55)==MACHINE_OK);
    assert(machine_step_back(m)==MACHINE_OK && machine_step_back(m)==MACHINE_OK);
    assert(bus_read8(&space,0x10)==0x33 && machine_newest(m)==3);
    assert(machine_poke(m,0x10,0x66)==MACHINE_OK && machine_newest(m)==2);
    assert(machine_step_back(m)==MACHINE_OK && bus_read8(&space,0x10)==0x33);
    assert(machine_set_pc(m,0x1234)==MACHINE_OK && cpu.PC==0x1234);
    assert(machine_step_back(m)==MACHINE_OK && cpu.PC==original.PC);
    assert(machine_step_back(m)==MACHINE_OK && bus_read8(&space,0x10)==0x11);
    assert(machine_step_back(m)==MACHINE_NO_HISTORY);
    machine_destroy(m);
}
static void test_eviction(void) {
    BYTE bytes[65536]={0}; physical_memory_t memory; address_space_t space; cpu_t cpu;
    assert(memory_init(&memory,bytes,sizeof bytes) && address_space_init(&space,&memory));
    for(unsigned i=0;i<65536;i++) bus_write8(&space,(WORD)i,0xEA);
    bus_write16(&space,0xFFFC,0x0800); assert(cpu_init(&cpu,&space));
    machine_t *m=machine_create(&cpu,4); assert(m);
    for(int i=0;i<20;i++) assert(machine_step(m)==MACHINE_OK);
    assert(machine_oldest(m)==16 && machine_position(m)==20 && machine_newest(m)==20);
    for(int i=0;i<4;i++) assert(machine_step_back(m)==MACHINE_OK);
    assert(machine_position(m)==16 && cpu.PC==0x0810 && machine_step_back(m)==MACHINE_NO_HISTORY);
    assert(machine_step(m)==MACHINE_OK && machine_newest(m)==17);
    for(int i=0;i<20;i++) assert(machine_step(m)==MACHINE_OK);
    assert(machine_oldest(m)==33 && machine_position(m)==37);
    machine_destroy(m);
    m=machine_create(&cpu,1); assert(m);
    assert(machine_step(m)==MACHINE_OK && machine_step(m)==MACHINE_OK);
    assert(machine_oldest(m)==1 && machine_step_back(m)==MACHINE_OK);
    assert(machine_step_back(m)==MACHINE_NO_HISTORY); machine_destroy(m);
}
int main(void) {
    test_roundtrip(); test_transactions(); test_eviction();
    puts("Machine undo/re-execution, atomic rollback, mapped/repeated writes, branching and eviction passed.");
    return 0;
}
