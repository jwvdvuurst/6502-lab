#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "6502cpu.h"
typedef struct { const cpu_t *expected; unsigned calls; } observer_t;
static void observe(const cpu_t *cpu, const char *mode, const char *instruction, void *data) {
    observer_t *o = data;
    assert(cpu == o->expected && mode && instruction);
    o->calls++;
}
int main(void) {
    BYTE ma[65536] = {0}, mb[65536] = {0};
    cpu_t a, b;
    physical_memory_t pa, pb;
    address_space_t sa, sb;
    assert(memory_init(&pa, ma, sizeof ma) && memory_init(&pb, mb, sizeof mb));
    assert(address_space_init(&sa, &pa) && address_space_init(&sb, &pb));
    const BYTE program[] = {0xA9,0x31,0x48,0xA9,0x00,0x68,0x85,0x10,0xE8,0x02};
    memcpy(ma+0x200, program, sizeof program);
    memcpy(mb+0x300, program, sizeof program); mb[0x301]=0x72;
    ma[0xFFFD]=2; mb[0xFFFD]=3;
    assert(!cpu_init(NULL, &sa) && !cpu_init(&a, NULL));
    assert(cpu_init(&a, &sa)); a.X=9;
    assert(cpu_init(&b, &sb) && a.X==9 && a.PC==0x200 && b.PC==0x300);
    observer_t oa={&a,0}, ob={&b,0};
    a.status_callback=observe; a.status_user_data=&oa;
    b.status_callback=observe; b.status_user_data=&ob;
    cpu_request_status(&a);
    assert(cpu_tick(&b) && ob.calls==0 && a.status_requested);
    assert(cpu_tick(&a) && oa.calls==1 && a.A==0x31 && b.A==0x72);
    cpu_request_status(&b);
    assert(cpu_tick(&a) && cpu_tick(&b));
    assert(ma[0x1FF]==0x31 && mb[0x1FF]==0x72 && a.stack_depth==1 && b.stack_depth==1);
    assert(ob.calls==1);
    for(int i=0;i<5;i++) assert(cpu_tick(&a) && cpu_tick(&b));
    assert(a.A==0x31 && b.A==0x72 && ma[0x10]==0x31 && mb[0x10]==0x72);
    assert(a.X==10 && b.X==1 && a.stack_depth==0 && b.stack_depth==0);
    /* Existing 0xAB alias is handled by the fallback switch. Preserve it. */
    cpu_request_status(&b);
    assert(cpu_execute(&b,0xAB) && b.Y==0x72 && a.Y==0 && ob.calls==2);
    cpu_t saved=b;
    BYTE saved_memory[65536]; memcpy(saved_memory, mb, sizeof mb);
    cpu_request_status(&a); cpu_reset(&a);
    assert(a.PC==0x200 && a.cycles==0 && a.SP==255 && a.P==0x20 && !a.status_requested);
    assert(a.status_callback==observe && a.status_user_data==&oa && ma[0x10]==0x31);
    assert(memcmp(&b,&saved,sizeof b)==0 && memcmp(mb,saved_memory,sizeof mb)==0);
    assert(cpu_test() && memcmp(&b,&saved,sizeof b)==0 && memcmp(mb,saved_memory,sizeof mb)==0);
    assert(cpu_init(&a,&sa) && !a.status_callback && !a.status_user_data);
    puts("CPU context isolation tests passed (table and reserved-opcode fallback).");
}


