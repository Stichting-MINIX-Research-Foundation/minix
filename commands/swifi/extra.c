/*
Declaration for Linux kernel compatibility
*/

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ptrace.h>

#include "extra.h"

pid_t victim_pid= -1;
char *victim_exe= NULL;

#define TRAP_BIT	(0x80000000)

static struct nlist *exe_nlist;
static int exe_nlist_n;

/* unsigned long __get_free_page(int type) { assert(0); } */
/* void *kmalloc(size_t size, int type) { assert(0); } */
void free_page(unsigned long page) { assert(0); }
/* void kfree(void *mem) { assert(0); } */
void vfree(void *mem) { assert(0); }

size_t strncpy_from_user(char *addr, const char *user_name, size_t size)
{ assert(0); return 0; }

/* void lock_kernel(void) { assert(0); } */
/* void unlock_kernel(void) { assert(0); } */
/* void __asm__(char *str) { assert(0); } */

extern void *__vmalloc(unsigned long size, int gfp_mask, pgprot_t prot)
{ assert(0); return NULL; }

#if 0
void kallsyms_sections(void *infop,
	int (*fp)(void *token, const char *modname, const char *secname,
	      ElfW(Addr) secstart, ElfW(Addr) secend, ElfW(Word) secflags))
{ assert(0); }
#endif

unsigned long __generic_copy_to_user(void *x, const void *y, unsigned long z)
{ assert(0); return -1; }
unsigned long __generic_copy_from_user(void *x, const void *y, unsigned long z)
{ assert(0); return -1; }

/* void read_lock(struct lock *lock) { assert(0); } */
/* void read_unlock(struct lock *lock) { assert(0); } */
void udelay(unsigned long usecs) { assert(0); }
int copy_to_user(void * result_record, void *res, size_t size)
{
	memcpy(result_record, res, size);
	return 0;
}

void panic(char *str) { assert(0); }

void printk(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

int kallsyms_address_to_symbol(db_expr_t off,
    const char * *mod_name, unsigned long *mod_start, unsigned long *mod_end, 
    const char * *sec_name, unsigned long *sec_start, unsigned long *sec_end, 
    const char * *sym_name, unsigned long *sym_start, unsigned long *sym_end)
{
	static char name[64];

	int i;
	unsigned long btext, etext;
	struct nlist *below, *above;

	off &= ~TRAP_BIT;
	load_nlist(victim_exe, &btext, &etext);
	below= above= NULL;
	for (i= 0; i<exe_nlist_n; i++)
	{
		if (exe_nlist[i].n_type != N_TEXT)
			continue;
		if (exe_nlist[i].n_value <= off)
		{
			if (!below || exe_nlist[i].n_value > below->n_value)
				below= &exe_nlist[i];
		}
		if (exe_nlist[i].n_value > off)
		{
			if (!above || exe_nlist[i].n_value < above->n_value)
				above= &exe_nlist[i];
		}
	}
#if 0
	if (below)
	{
		printf("found '%s' at 0x%x\n", below->n_name, below->n_value);
	}
	if (above)
	{
		printf("found '%s' at 0x%x\n", above->n_name, above->n_value);
	}
#endif

	btext |= TRAP_BIT;
	etext |= TRAP_BIT;

	*mod_name = victim_exe;
	*mod_start = btext;
	*mod_end = etext;
	*sec_name = ".text";
	*sec_start = btext;
	*sec_end = etext;

	assert(below && above);

	strncpy(name, below->n_name, sizeof(name)-1);
	name[sizeof(name)-1]= '\0';
	*sym_name= name;

	*sym_start= below->n_value | TRAP_BIT;
	*sym_end= above->n_value | TRAP_BIT;

	return 1;
}

struct module *module_list;
struct task_struct *task_list;
struct lock tasklist_lock;

unsigned long text_read_ul(void *addr)
{
	int i;
	unsigned long value;

	for (i= 0; i<sizeof(value); i++)
	{
		((unsigned char *)&value)[i]= text_read_ub((char *)addr+i);
	}
	return value;
}

unsigned char text_read_ub(void *addr)
{
	int v;
	unsigned long vaddr;

	vaddr= (unsigned long)addr;
	vaddr &= ~TRAP_BIT;
	v= ptrace(T_READB_INS, victim_pid, vaddr, 0);
	if (v < 0)
	{
		fprintf(stderr,
	"text_read_ub: trace T_READB_INS failed on pid %d, addr 0x%lx: %s\n",
			victim_pid, vaddr, strerror(errno));
		exit(1);
	}
	return v;
}

void text_write_ul(void *addr, unsigned long value)
{
	int i;

	for (i= 0; i<sizeof(value); i++)
	{
		text_write_ub((char *)addr+i, ((unsigned char *)&value)[i]);
	}
}

void text_write_ub(void *addr, unsigned char value)
{
	int v;
	unsigned long vaddr;

	vaddr= (unsigned long)addr;
	vaddr &= ~TRAP_BIT;
	v= ptrace(T_WRITEB_INS, victim_pid, vaddr, value);
	if (v < 0)
	{
		fprintf(stderr,
	"text_read_ub: trace T_WRITEB_INS failed on pid %d, addr 0x%lx: %s\n",
			victim_pid, vaddr, strerror(errno));
		exit(1);
	}
}

void load_nlist(exe_name, btextp, etextp)
char *exe_name;
unsigned long *btextp;
unsigned long *etextp;
{
	int i;
	unsigned long btext, etext;

	if (!exe_nlist)
	{
		exe_nlist_n= read_nlist(exe_name, &exe_nlist);
		if (exe_nlist_n <= 0)
		{
			if (exe_nlist_n == -1)
			{
				fprintf(stderr,
				"error reading name list from '%s': %s\n",
					exe_name, strerror(errno));
			}
			else
				fprintf(stderr, "no name list in '%s'\n",
					exe_name);
			exit(1);
		}
	}

	if (!btextp && !etextp)
		return;

	etext= 0;
	btext= (unsigned long)-1;
	for (i= 0; i<exe_nlist_n; i++)
	{
		if (exe_nlist[i].n_type != N_TEXT)
			continue;
		if (exe_nlist[i].n_value < btext)
			btext= exe_nlist[i].n_value;
		if (exe_nlist[i].n_value > etext)
			etext= exe_nlist[i].n_value;
	}

	if (btext >= etext)
	{
		fprintf(stderr, "Bad btext (0x%lx) or etext (0x%lx) in %s\n",
			btext, etext, exe_name);
		exit(1);
	}

	btext |= TRAP_BIT;
	etext |= TRAP_BIT;

	if (btextp)
		*btextp= btext;
	if (etextp)
		*etextp= etext;
}
