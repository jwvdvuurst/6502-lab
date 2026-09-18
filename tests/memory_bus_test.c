#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "6502cpu.h"
#include "6502framebuffer.h"
typedef struct { WORD block, offset; BYTE before, after; } write_t;
typedef struct { write_t writes[4096]; size_t count; } log_t;
static void observe(const physical_memory_t *memory, WORD block, WORD offset,
                    BYTE before, BYTE after, void *data) {
    log_t *log = data;
    assert(log->count < 4096);
    assert(memory_read8(memory, block, offset) == before);
    log->writes[log->count++] = (write_t){block, offset, before, after};
}
static void test_bus(void) {
    BYTE bytes[68 * 1024] = {0};
    physical_memory_t memory;
    address_space_t space;
    assert(memory_init(&memory, bytes, sizeof bytes));
    assert(address_space_init(&space, &memory));
    /* Check all logical addresses and all 64 window edges, including wrapping. */
    for (unsigned i=0;i<65536;i++) bus_write8(&space,(WORD)i,(BYTE)(i ^ (i >> 8)));
    for (unsigned i=0;i<65536;i++) assert(bytes[i]==(BYTE)(i ^ (i >> 8)));
    for (unsigned i=0;i<64;i++) {
        WORD edge=(WORD)(i*1024+1023);
        bus_write16(&space,edge,(WORD)(0xA100+i));
        assert(bus_read16(&space,edge)==0xA100+i);
        assert(bytes[edge]==i && bytes[(WORD)(edge+1)]==0xA1);
    }
    assert(address_space_map(&space,32,66) && address_space_map(&space,33,64));
    bus_write16(&space,0x83FF,0xBEEF);
    assert(bytes[66*1024+1023]==0xEF && bytes[64*1024]==0xBE);
    assert(bus_read16(&space,0x83FF)==0xBEEF);
    assert(address_space_map(&space,63,65) && address_space_map(&space,0,67));
    bus_write16(&space,0xFFFF,0x1234);
    assert(bytes[65*1024+1023]==0x34 && bytes[67*1024]==0x12);
    assert(bus_read16(&space,0xFFFF)==0x1234);
    /* Unmapped storage is retained; alias windows see the same physical byte. */
    assert(address_space_map(&space,32,64));
    assert(bus_read8(&space,0x8000)==0xBE);
    bus_write8(&space,0x8000,0x77);
    assert(bus_read8(&space,0x8400)==0x77);
    assert(address_space_map(&space,32,66));
    assert(bus_read8(&space,0x83FF)==0xEF);
    address_space_t saved=space;
    assert(!address_space_map(&space,64,0) && !address_space_map(&space,0,68));
    assert(!address_space_map(&space,0,65536) && !address_space_map(NULL,0,0));
    assert(memcmp(&saved,&space,sizeof space)==0);
    physical_memory_t saved_memory=memory;
    assert(!memory_init(&memory,NULL,1024) && !memory_init(&memory,bytes,0));
    assert(!memory_init(&memory,bytes,1025) && !memory_init(&memory,bytes,(size_t)65537*1024));
    assert(memcmp(&saved_memory,&memory,sizeof memory)==0);
    physical_memory_t small;
    assert(memory_init(&small,bytes,1024) && !address_space_init(&space,&small));
    assert(memcmp(&saved,&space,sizeof space)==0);
    log_t log={0}; memory.write_observer=observe; memory.observer_data=&log;
    bus_write16(&space,0xFFFF,0x5678);
    assert(log.count==2 && log.writes[0].block==65 && log.writes[0].offset==1023);
    assert(log.writes[0].before==0x34 && log.writes[0].after==0x78);
    assert(log.writes[1].block==67 && log.writes[1].offset==0);
    assert(log.writes[1].before==0x12 && log.writes[1].after==0x56);
    bus_write8(&space,0,0x56);
    assert(log.count==3 && log.writes[2].before==log.writes[2].after);
}
static void test_cpu_bus(void) {
    BYTE bytes[68*1024]={0};
    physical_memory_t memory; address_space_t space; cpu_t cpu;
    assert(memory_init(&memory,bytes,sizeof bytes) && address_space_init(&space,&memory));
    assert(cpu_init(&cpu,&space));
    /* Change mapping after CPU initialization: no cached stack or memory pointer. */
    assert(address_space_map(&space,0,64) && address_space_map(&space,63,65));
    assert(address_space_map(&space,2,66) && address_space_map(&space,3,67));
    bus_write16(&space,0xFFFC,0x0800); cpu_reset(&cpu);
    assert(cpu.PC==0x0800 && space.blocks[0]==64 && space.blocks[63]==65);
    const BYTE code[]={0x20,0x10,0x08,0xEA};
    for(size_t i=0;i<sizeof code;i++) bus_write8(&space,(WORD)(0x800+i),code[i]);
    bus_write8(&space,0x0810,0x20); bus_write16(&space,0x0811,0x0820);
    bus_write8(&space,0x0813,0x60); bus_write8(&space,0x0820,0x60);
    log_t log={0}; memory.write_observer=observe; memory.observer_data=&log;
    assert(cpu_tick(&cpu) && cpu.PC==0x0810 && cpu.stack_depth==2);
    assert(cpu_tick(&cpu) && cpu.PC==0x0820 && cpu.stack_depth==4);
    assert(log.count==4 && log.writes[0].block==64 && log.writes[0].offset==0x1FF);
    assert(log.writes[0].after==8 && log.writes[1].after==2);
    assert(bytes[0x1FF]==0 && bytes[64*1024+0x1FF]==8);
    assert(cpu_tick(&cpu) && cpu.PC==0x0813);
    assert(cpu_tick(&cpu) && cpu.PC==0x0803 && cpu.stack_depth==0 && cpu.SP==255);
    /* Preserve bounded stack policy: handlers ignore rejected push/pop results. */
    log.count=0;
    for(unsigned i=0;i<256;i++) { cpu.A=(BYTE)i; assert(cpu_execute(&cpu,0x48)); }
    assert(cpu.SP==255 && cpu.stack_depth==256 && log.count==256);
    assert(cpu_execute(&cpu,0x48) && cpu.stack_depth==256 && log.count==256);
    for(int i=255;i>=0;i--) { assert(cpu_execute(&cpu,0x68) && cpu.A==(BYTE)i); }
    assert(cpu.stack_depth==0 && cpu.SP==255 && cpu_execute(&cpu,0x68));
    memory.write_observer=NULL;
    /* Fetch and operand access straddle nonadjacent physical blocks. */
    bus_write8(&space,0x0BFF,0xA9); bus_write8(&space,0x0C00,0x5A);
    cpu.PC=0x0BFF; assert(cpu_tick(&cpu) && cpu.A==0x5A && cpu.PC==0x0C01);
    bus_write8(&space,0x0BFE,0xAD); bus_write16(&space,0x0BFF,0x1234);
    bus_write8(&space,0x1234,0x91);
    cpu.PC=0x0BFE; assert(cpu_tick(&cpu) && cpu.A==0x91 && cpu.PC==0x0C01);
    bus_write8(&space,0xFFFF,0xA9); bus_write8(&space,0,0x42);
    cpu.PC=0xFFFF; assert(cpu_tick(&cpu) && cpu.A==0x42 && cpu.PC==1);
    /* Zero-page pointer wraps at $FF, not at a 1 KiB boundary. */
    bus_write8(&space,0x00FF,0x34); bus_write8(&space,0,0x12);
    bus_write8(&space,0x0100,0xEE); bus_write8(&space,0x0800,0xB1);
    bus_write8(&space,0x0801,0xFF); cpu.PC=0x0800; cpu.Y=0;
    assert(cpu_tick(&cpu) && cpu.A==0x91);
    /* Store and read-modify-write reach the mapped physical block and observer. */
    bus_write8(&space,0x0800,0x85); bus_write8(&space,0x0801,0x10);
    bus_write8(&space,0x0802,0xE6); bus_write8(&space,0x0803,0x10);
    log.count=0; memory.write_observer=observe; cpu.PC=0x0800; cpu.A=0x25;
    assert(cpu_tick(&cpu) && cpu_tick(&cpu));
    assert(log.count==2 && log.writes[0].block==64 && log.writes[0].offset==0x10);
    assert(log.writes[1].before==0x25 && log.writes[1].after==0x26 && bytes[0x10]==0);
}
static void test_framebuffer(void) {
    BYTE bytes[65*1024]={0};
    physical_memory_t memory; address_space_t space; framebuffer_t fb;
    assert(memory_init(&memory,bytes,sizeof bytes) && address_space_init(&space,&memory));
    assert(framebuffer_init(&fb,&memory,48));
    assert(address_space_map(&space,48,64));
    bus_write8(&space,0xC000,0x99);
    log_t log={0}; memory.write_observer=observe; memory.observer_data=&log;
    assert(framebuffer_clear(&fb) && log.count==1024);
    assert(bytes[64*1024]==0x99 && framebuffer_read8(&fb,0)==0x20);
    for(size_t i=0;i<log.count;i++) assert(log.writes[i].block==48 && log.writes[i].offset==i);
    log.count=0;
    assert(framebuffer_write_char(&fb,39,24,'Z'));
    assert(framebuffer_read8(&fb,999)=='Z' && log.count==1 && log.writes[0].offset==999);
    assert(!framebuffer_write_char(&fb,40,24,'X') && !framebuffer_write_char(&fb,0,25,'X'));
    assert(!framebuffer_write_string(&fb,39,24,"XX") && !framebuffer_write_string(&fb,0,0,NULL));
    assert(log.count==1 && bytes[49*1024]==0);
    char long_text[301]; memset(long_text,'L',300); long_text[300]=0;
    assert(framebuffer_write_string(&fb,0,0,long_text));
    assert(framebuffer_read8(&fb,299)=='L');
    assert(framebuffer_write_char(&fb,0,1,'Q'));
    log.count=0; assert(framebuffer_scroll(&fb,1) && log.count==1000);
    assert(framebuffer_read8(&fb,0)=='Q' && framebuffer_read8(&fb,959)=='Z');
    for(WORD i=960;i<1000;i++) assert(framebuffer_read8(&fb,i)==0x20);
    log.count=0; assert(framebuffer_scroll(&fb,0) && log.count==0);
    assert(framebuffer_scroll(&fb,24) && log.count==1024);
    framebuffer_t saved=fb;
    assert(!framebuffer_init(&fb,&memory,65) && !framebuffer_init(&fb,NULL,0));
    assert(memcmp(&fb,&saved,sizeof fb)==0);
    assert(bytes[64*1024]==0x99);
    assert(!framebuffer_clear(NULL) && !framebuffer_scroll(NULL,1));
}
int main(void) {
    test_bus(); test_cpu_bus(); test_framebuffer();
    puts("Phase 2 bus, mapped CPU/stack, write observation and framebuffer tests passed.");
    return 0;
}

