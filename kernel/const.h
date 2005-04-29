/* General constants used by the kernel. */

#include <ibm/interrupt.h>	/* interrupt numbers and hardware vectors */
#include <ibm/ports.h>		/* port addresses and magic numbers */
#include <ibm/bios.h>		/* BIOS addresses, sizes and magic numbers */

#if (CHIP == INTEL)

/* To translate an address in kernel space to a physical address.  This is
 * the same as umap_local(proc_ptr, D, vir, sizeof(*vir)), but less costly.
 */
#define vir2phys(vir)	(kinfo.data_base + (vir_bytes) (vir))

/* Constants used in virtual_copy(). Values must be 0 and 1, respectively! */
#define _SRC_	0
#define _DST_	1

/* Translate a pointer to a field in a structure to a pointer to the structure
 * itself.  So it translates '&struct_ptr->field' back to 'struct_ptr'.
 */
#define structof(type, field, ptr) \
	((type *) (((char *) (ptr)) - offsetof(type, field)))

/* How many bytes for the kernel stack. Space allocated in mpx.s. */
#define K_STACK_BYTES   1024	

/* How many bytes for (port,value)-pairs vector to copy in. */
#define VDEVIO_BUF_SIZE  128

/* How many elements in vector of virtual copy requests. */
#define VCOPY_VEC_SIZE    16

/* Program stack words and masks. */
#define INIT_PSW      0x0200	/* initial psw */
#define INIT_TASK_PSW 0x1200	/* initial psw for tasks (with IOPL 1) */
#define TRACEBIT       0x100	/* OR this with psw in proc[] for tracing */
#define SETPSW(rp, new)	/* permits only certain bits to be set */ \
	((rp)->p_reg.psw = (rp)->p_reg.psw & ~0xCD5 | (new) & 0xCD5)
#define IF_MASK 0x00000200
#define IOPL_MASK 0x003000

/* Disable/Enable hardware interrupts. */
#define lock()		intr_disable()
#define unlock()	intr_enable()

/* Sizes of memory tables. The boot monitor distinguishes three memory areas, 
 * namely low mem below 1M, 1M-16M, and mem after 16M. More chunks are needed
 * for DOS MINIX.
 */
#define NR_MEMS            8	/* number of chunks of memory */


#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific constants go here. */
#endif /* (CHIP == M68000) */

