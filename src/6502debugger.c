#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "6502debugger.h"
static void help(FILE *out) {
    fputs("Commands (addresses/bytes hexadecimal, counts decimal):\n"
          "  step [n]       execute n instructions (default 1)\n"
          "  back [n]       undo n instructions/edits (default 1)\n"
          "  run [n]        run up to n instructions (default 10000)\n"
          "  break addr     stop run BEFORE this address; break off clears it\n"
          "  regs           registers and three next bytes (not disassembly)\n"
          "  mem addr [n]   show up to 256 bytes (default 32)\n"
          "  poke addr byte write one byte, undoable\n"
          "  pc addr        change PC, undoable\n"
          "  history        current/oldest/newest retained positions\n"
          "  screen         show the physical 40x25 framebuffer\n"
          "  help / quit    commands / exit (EOF also exits)\n"
          "Run/step/back counts are limited to 1000000. Step ignores breakpoints.\n",out);
}
static bool number(const char *s, int base, unsigned long max, unsigned long *value) {
    if (!s || !*s || *s=='-' || *s=='+') return false;
    char *end; errno=0;
    unsigned long parsed=strtoul(s,&end,base);
    if (errno || end==s || *end || parsed>max) return false;
    *value=parsed; return true;
}
static void registers(machine_t *m, FILE *out) {
    const cpu_t *cpu=machine_cpu(m);
    fprintf(out,"PC=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X cycles=%llu depth=%u\n",
        cpu->PC,cpu->A,cpu->X,cpu->Y,cpu->SP,cpu->P,cpu->cycles,cpu->stack_depth);
    fprintf(out,"Next bytes: %02X %02X %02X\n",bus_read8(cpu->address_space,cpu->PC),
        bus_read8(cpu->address_space,(WORD)(cpu->PC+1)),bus_read8(cpu->address_space,(WORD)(cpu->PC+2)));
}
static void history(machine_t *m,FILE *out) {
    fprintf(out,"History position=%llu oldest=%llu newest=%llu capacity=%zu\n",
        machine_position(m),machine_oldest(m),machine_newest(m),machine_capacity(m));
}
bool debugger_run(machine_t *m,const framebuffer_t *fb,FILE *in,FILE *out) {
    if (!m || !fb || !in || !out) return false;
    bool breakpoint_set=false; WORD breakpoint=0;
    char line[256];
    fputs("6502 Lab debugger. Type help for commands.\n",out);
    registers(m,out); history(m,out);
    while (true) {
        fputs("dbg> ",out); fflush(out);
        if (!fgets(line,sizeof line,in)) break;
        if (!strchr(line,'\n') && !feof(in)) {
            int ch; while ((ch=fgetc(in))!='\n' && ch!=EOF) {}
            fputs("Command too long; ignored.\n",out); continue;
        }
        char *words[4]; size_t count=0;
        char *word=strtok(line," \t\r\n");
        while(word && count<4) { words[count++]=word; word=strtok(NULL," \t\r\n"); }
        if (!count) continue;
        const char *cmd=words[0];
        unsigned long a=0,b=0;
        if (count==1 && !strcmp(cmd,"quit")) break;
        if (count==1 && !strcmp(cmd,"help")) { help(out); continue; }
        if (count==1 && !strcmp(cmd,"regs")) { registers(m,out); continue; }
        if (count==1 && !strcmp(cmd,"history")) { history(m,out); continue; }
        if (count==1 && !strcmp(cmd,"screen")) {
            for (WORD y=0;y<25;y++) {
                for(WORD x=0;x<40;x++) {
                    BYTE ch=framebuffer_read8(fb,(WORD)(y*40+x));
                    fputc(ch>=32 && ch<=126 ? ch : '.',out);
                }
                fputc('\n',out);
            }
            continue;
        }
        if (count==2 && !strcmp(cmd,"break")) {
            if (!strcmp(words[1],"off")) {
                breakpoint_set=false; fputs("Breakpoint cleared.\n",out); continue;
            }
            if (number(words[1],16,65535,&a)) {
                breakpoint=(WORD)a; breakpoint_set=true;
                fprintf(out,"Breakpoint at %04X.\n",breakpoint); continue;
            }
        }
        if ((count==2 || count==3) && !strcmp(cmd,"mem") && number(words[1],16,65535,&a)) {
            b=32;
            if (count==3 && (!number(words[2],10,256,&b) || !b)) goto invalid;
            for(unsigned long i=0;i<b;i++) {
                if(i%16==0) fprintf(out,"%04X:",(WORD)(a+i));
                fprintf(out," %02X",bus_read8(machine_cpu(m)->address_space,(WORD)(a+i)));
                if(i%16==15 || i+1==b) fputc('\n',out);
            }
            continue;
        }
        if (count==3 && !strcmp(cmd,"poke") && number(words[1],16,65535,&a) && number(words[2],16,255,&b)) {
            fprintf(out,"%s\n",machine_result_text(machine_poke(m,(WORD)a,(BYTE)b)));
            history(m,out); continue;
        }
        if (count==2 && !strcmp(cmd,"pc") && number(words[1],16,65535,&a)) {
            fprintf(out,"%s\n",machine_result_text(machine_set_pc(m,(WORD)a)));
            registers(m,out); continue;
        }
        if (count<=2 && (!strcmp(cmd,"step") || !strcmp(cmd,"back") || !strcmp(cmd,"run"))) {
            bool running=!strcmp(cmd,"run"), backward=!strcmp(cmd,"back");
            a=running ? 10000 : 1;
            if(count==2 && (!number(words[1],10,1000000,&a) || !a)) goto invalid;
            unsigned long done=0;
            while(done<a) {
                if(running && breakpoint_set && machine_cpu(m)->PC==breakpoint) {
                    fprintf(out,"Breakpoint hit at %04X.\n",breakpoint); break;
                }
                machine_result_t result=backward ? machine_step_back(m) : machine_step(m);
                if(result!=MACHINE_OK) { fprintf(out,"%s\n",machine_result_text(result)); break; }
                done++;
            }
            fprintf(out,"%s %lu.\n",backward ? "Undid" : "Executed",done);
            registers(m,out); history(m,out); continue;
        }
invalid:
        fputs("Invalid command or argument. Type help.\n",out);
    }
    fputs("Debugger closed.\n",out);
    return !ferror(in) && !ferror(out);
}
