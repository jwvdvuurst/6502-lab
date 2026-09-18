#include <stdio.h>
#include <stdint.h>
#include "6502cpu.h"
#include "6502memory.h"
#ifdef TRACE_MACHINE
#include <assert.h>
#include <string.h>
#include "6502machine.h"
#endif
static BYTE memory[65536];
#undef current_instruction
#undef cycles
static uint64_t hash_memory(void) {
 uint64_t h = UINT64_C(14695981039346656037);
 for (unsigned i=0;i<65536;i++) { h ^= memory[i]; h *= UINT64_C(1099511628211); }
 return h;
}
int main(int argc,char **argv) {
 if(argc!=2 || !memory_test() || !cpu_test()) return 1;
 physical_memory_t physical; address_space_t space;
 if (!memory_init(&physical, memory, sizeof memory) || !address_space_init(&space, &physical)) return 4;
 for(unsigned i=0;i<65536;i++) bus_write8(&space, (WORD)i, 0xEA);
 /* Reproduce historical cpu_test stack residue for an identical initial image. */
 for(unsigned i=0;i<256;i++) bus_write8(&space, (WORD)(0x100+i), (BYTE)(255-i));
 FILE *f=fopen(argv[1],"rb"); if(!f)return 2;
 unsigned address=0xD000; int byte;
 while(address<65536 && (byte=fgetc(f))!=EOF) bus_write8(&space, (WORD)address++, (BYTE)byte);
 fclose(f); bus_write16(&space,0xFFFC,0xD000); cpu_t cpu; if (!cpu_init(&cpu, &space)) return 4;
#ifdef TRACE_MACHINE
 machine_t *machine=machine_create(&cpu,1000); if(!machine)return 5;
#endif
 for(unsigned i=0;i<=1000000;i++){
#ifdef TRACE_MACHINE
  if(i && i%1000==0) {
   cpu_t saved=cpu; BYTE saved_memory[65536]; memcpy(saved_memory,memory,sizeof memory);
   for(unsigned j=0;j<1000;j++)assert(machine_step_back(machine)==MACHINE_OK);
   for(unsigned j=0;j<1000;j++)assert(machine_step(machine)==MACHINE_OK);
   assert(memcmp(memory,saved_memory,sizeof memory)==0);
   assert(cpu.A==saved.A && cpu.X==saved.X && cpu.Y==saved.Y && cpu.P==saved.P);
   assert(cpu.PC==saved.PC && cpu.SP==saved.SP && cpu.stack_depth==saved.stack_depth);
   assert(cpu.cycles==saved.cycles && cpu.current_instruction==saved.current_instruction);
   assert(cpu.status_requested==saved.status_requested && machine_position(machine)==i);
  }
#endif
  if(i%1000==0)printf("%u %02X %02X %02X %02X %02X %04X %02X %llu %u %016llX\n",
   i,cpu.A,cpu.X,cpu.Y,cpu.SP,cpu.P,cpu.PC,cpu.current_instruction,cpu.cycles,
   cpu.stack_depth,(unsigned long long)hash_memory());
#ifdef TRACE_MACHINE
  if(i<1000000 && machine_step(machine)!=MACHINE_OK)return 3;
#else
  if(i<1000000 && !cpu_tick(&cpu))return 3;
#endif
 }
#ifdef TRACE_MACHINE
 machine_destroy(machine);
#endif
 return 0;
}



