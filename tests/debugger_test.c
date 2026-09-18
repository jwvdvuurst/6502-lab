#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "6502debugger.h"
static void session(machine_t *m,framebuffer_t *fb,const char *script,char *output,size_t size) {
    FILE *input=tmpfile(), *out=tmpfile(); assert(input && out);
    fputs(script,input); rewind(input);
    assert(debugger_run(m,fb,input,out)); fflush(out); rewind(out);
    size_t count=fread(output,1,size-1,out); output[count]=0;
    assert(!ferror(out)); fclose(input); fclose(out);
}
int main(void) {
    BYTE bytes[65536]={0}; physical_memory_t memory; address_space_t space; cpu_t cpu; framebuffer_t fb;
    assert(memory_init(&memory,bytes,sizeof bytes) && address_space_init(&space,&memory));
    const BYTE program[]={0xA9,0x2A,0x85,0x10,0xE8,0x4C,0x00,0x08};
    for(size_t i=0;i<sizeof program;i++) bus_write8(&space,(WORD)(0x800+i),program[i]);
    bus_write16(&space,0xFFFC,0x0800); assert(cpu_init(&cpu,&space));
    assert(framebuffer_init(&fb,&memory,48));
    machine_t *m=machine_create(&cpu,16); assert(m);
    char output[16384];
    session(m,&fb,"help\nbreak 0804\nrun 10\nmem 10 1\nback\nmem 10 1\n"
        "step\nstep\nback\nhistory\nbreak off\npoke c000 41\nscreen\nback\n"
        "pc 0900\nback\nregs\nquit\n",output,sizeof output);
    assert(strstr(output,"Breakpoint hit at 0804") && strstr(output,"Executed 2."));
    assert(strstr(output,"0010: 2A") && strstr(output,"0010: 00"));
    assert(strstr(output,"PC=0804") && strstr(output,"A=2A") && strstr(output,"Breakpoint cleared"));
    assert(cpu.PC==0x0804 && cpu.X==0 && bus_read8(&space,0x10)==0x2A && bytes[0xC000]==0);
    assert(machine_position(m)==2);
    unsigned long long before=machine_position(m);
    session(m,&fb,"step -1\nstep 0\nrun 1000001\nback 999999999999999999999\n"
        "mem 10000\nmem 0 257\npoke 10 100\npoke -1 2\npoke 10 2 extra\n"
        "pc xyz\nrun 2 extra\nbreak ffff junk\nquit\n",output,sizeof output);
    assert(machine_position(m)==before && cpu.PC==0x0804 && strstr(output,"Invalid command"));
    char long_line[400]; memset(long_line,'x',sizeof long_line); long_line[398]='\n'; long_line[399]=0;
    session(m,&fb,long_line,output,sizeof output);
    assert(strstr(output,"Command too long") && machine_position(m)==before);
    session(m,&fb,"back 10\n",output,sizeof output);
    assert(strstr(output,"Oldest retained state reached") && machine_position(m)==0 && cpu.PC==0x800);
    session(m,&fb,"run 3",output,sizeof output); /* Last line without newline and EOF. */
    assert(strstr(output,"Executed 3.") && cpu.PC==0x805 && cpu.X==1);
    machine_destroy(m);
    puts("Debugger scripts: breakpoint, stepping/back, inspection, edits, malformed input and EOF passed.");
    return 0;
}
