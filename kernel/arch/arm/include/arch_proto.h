
#ifndef _ARM_PROTO_H
#define _ARM_PROTO_H

#include <machine/vm.h>

#define K_STACK_SIZE	ARM_PAGE_SIZE


#ifndef __ASSEMBLY__

#include "cpufunc.h"

/* klib */
__dead void reset(void);
phys_bytes vir2phys(void *);
vir_bytes phys_memset(phys_bytes ph, u32_t c, phys_bytes bytes);

void __switch_address_space(struct proc *p, struct proc **__ptproc);
#define switch_address_space(proc)	\
	__switch_address_space(proc, get_cpulocal_var_ptr(ptproc))

void __copy_msg_from_user_end(void);
void __copy_msg_to_user_end(void);
void __user_copy_msg_pointer_failure(void);

/* multiboot.c */
void multiboot_init(void);

/* protect.c */
struct tss_s {
  reg_t sp0;                    /* stack pointer to use during interrupt */
} __attribute__((packed));
int tss_init(unsigned cpu, void * kernel_stack);

void add_memmap(kinfo_t *cbi, u64_t addr, u64_t len);
phys_bytes alloc_lowest(kinfo_t *cbi, phys_bytes len);
void vm_enable_paging(void);
void cut_memmap(kinfo_t *cbi, phys_bytes start, phys_bytes end);
phys_bytes pg_roundup(phys_bytes b);
void pg_info(reg_t *, u32_t **);
void pg_clear(void);
void pg_identity(kinfo_t *);
phys_bytes pg_load(void);
void pg_map(phys_bytes phys, vir_bytes vaddr, vir_bytes vaddr_end, kinfo_t *cbi);
int pg_mapkernel(void);
void pg_mapproc(struct proc *p, struct boot_image *ip, kinfo_t *cbi);

EXTERN void * k_stacks_start;
extern void * k_stacks;

#define get_k_stack_top(cpu)	((void *)(((char*)(k_stacks)) \
					+ 2 * ((cpu) + 1) * K_STACK_SIZE))


/* functions defined in architecture-independent kernel source. */
#include "kernel/proto.h"

#endif /* __ASSEMBLY__ */

#endif
