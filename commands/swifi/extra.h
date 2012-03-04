/*
Compatibility with the linux kernel environment 
*/

#include <nlist.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#if 0
struct module
{
	struct module *next;
	char *name;
};
extern struct module *module_list;
#endif

struct thread
{
	unsigned long esp;
};

struct task_struct
{
	struct thread thread;
	struct task_struct *next;
};

unsigned long __get_free_page(int type);
void *kmalloc(size_t size, int type);
#define GFP_KERNEL 1
void free_page(unsigned long page);
void kfree(void *mem);
void vfree(void *mem);

size_t strncpy_from_user(char *addr, const char *user_name, size_t size);

void lock_kernel(void);
void unlock_kernel(void);

/* void __asm__(char *str); */

#define for_each_task(t) for(t= task_list; t; t=t->next)
extern struct task_struct *task_list;

typedef struct { int foo; } pgprot_t;
extern void *__vmalloc(unsigned long size, int gfp_mask, pgprot_t prot);

#define ElfW(type) Elf_ ## type
typedef unsigned long Elf_Addr;
typedef unsigned long Elf_Word;

void kallsyms_sections(void *infop,
	int (*fp)(void *token, const char *modname, const char *secname,
	      ElfW(Addr) secstart, ElfW(Addr) secend, ElfW(Word) secflags));

unsigned long __generic_copy_to_user(void *, const void *, unsigned long);
unsigned long __generic_copy_from_user(void *, const void *, unsigned long);

struct lock { int dummy; };
extern struct lock tasklist_lock;
void read_lock(struct lock *lock);
void read_unlock(struct lock *lock);

void udelay(unsigned long usecs);

int copy_to_user(void * result_record, void *res, size_t size);

void panic(char *str);

#define PAGE_SIZE	(0x1000)
#define PAGE_MASK	(0x0fff)
#define PAGE_OFFSET	0	/* What does this do? */
#define TASK_SIZE	0	/* What does this do? */

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
