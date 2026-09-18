/* Exercise the actual CLI loader against a nonidentity bus without opening UI. */
#define main emulator_main
#include "../src/6502test.c"
#undef main
#include <assert.h>
typedef struct { unsigned count; WORD block[16], offset[16]; } loader_log_t;
static void observed(const physical_memory_t *memory, WORD block, WORD offset,
                     BYTE before, BYTE after, void *data) {
    loader_log_t *log=data;
    (void)after;
    assert(memory_read8(memory,block,offset)==before && log->count<16);
    log->block[log->count]=block; log->offset[log->count++]=offset;
}
static void fixture(const char *path,const BYTE *bytes,size_t length) {
    FILE *f=fopen(path,"wb"); assert(f);
    assert(fwrite(bytes,1,length,f)==length); assert(fclose(f)==0);
}
int main(void) {
    BYTE bytes[67*1024]={0}; physical_memory_t memory; address_space_t space;
    assert(memory_init(&memory,bytes,sizeof bytes) && address_space_init(&space,&memory));
    assert(address_space_map(&space,32,64) && address_space_map(&space,33,65));
    assert(address_space_map(&space,63,66));
    loader_log_t log={0}; memory.write_observer=observed; memory.observer_data=&log;
    const BYTE bin[]={0xA9,0x42,0x85,0x10};
    fixture("build/phase2/loader.bin",bin,sizeof bin);
    assert(load_binary_file(&space,"build/phase2/loader.bin",0x83FF,true));
    assert(log.count==6 && log.block[0]==64 && log.offset[0]==1023);
    assert(log.block[1]==65 && log.offset[1]==0 && log.block[4]==66 && log.offset[4]==0x3FC);
    assert(bus_read8(&space,0x83FF)==0xA9 && bus_read8(&space,0x8400)==0x42);
    assert(bytes[0x83FF]==0 && bus_read16(&space,0xFFFC)==0x83FF);
    const BYTE prg[]={0xFF,0x83,0xEA,0x60};
    fixture("build/phase2/loader.prg",prg,sizeof prg); log.count=0;
    assert(load_binary_file(&space,"build/phase2/loader.prg",0,false));
    assert(log.count==4 && bus_read8(&space,0x83FF)==0xEA && bus_read8(&space,0x8400)==0x60);
    /* Preserve loader's stop-at-$FFFF behavior, rather than wrapping file data. */
    log.count=0;
    assert(load_binary_file(&space,"build/phase2/loader.bin",0xFFFF,true));
    assert(log.count==3 && bus_read8(&space,0xFFFF)==0xA9 && bus_read8(&space,0)==0);
    fixture("build/phase2/short.prg",prg,1); log.count=0;
    assert(!load_binary_file(&space,"build/phase2/short.prg",0,false) && log.count==0);
    assert(!load_binary_file(&space,"build/phase2/unused.asm",0,false) && log.count==0);
    puts("Binary/PRG loader bus routing, boundaries and validation tests passed.");
    return 0;
}
