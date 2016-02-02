/*
Compatibility with the linux kernel environment 
*/

#include <nlist.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#define ElfW(type) Elf_ ## type
typedef unsigned long Elf_Addr;
typedef unsigned long Elf_Word;

void kallsyms_sections(void *infop,
	int (*fp)(void *token, const char *modname, const char *secname,
	      ElfW(Addr) secstart, ElfW(Addr) secend, ElfW(Word) secflags));

void printk(char *fmt, ...);

#include "ddb.h"
#include "db_machdep.h"
int kallsyms_address_to_symbol(db_expr_t off,
    const char * *mod_name, unsigned long *mod_start, unsigned long *mod_end, 
    const char * *sec_name, unsigned long *sec_start, unsigned long *sec_end, 
    const char * *sym_name, unsigned long *sym_start, unsigned long *sym_end);

int read_nlist(const char *filenamer, struct nlist **nlist_table);

unsigned long text_read_ul(void *addr);
unsigned char text_read_ub(void *addr);
void text_write_ul(void *addr, unsigned long value);
void text_write_ub(void *addr, unsigned char value);

extern pid_t victim_pid;
extern char *victim_exe;

void load_nlist(char *exe_name, unsigned long *btextp, unsigned long *etextp);
