/* General constants used by the kernel. */

#include <ibm/interrupt.h>	/* interrupt numbers and hardware vectors */
#include <ibm/ports.h>		/* port addresses and magic numbers */
#include <ibm/bios.h>		/* BIOS addresses, sizes and magic numbers */
#include <minix/config.h>

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

/* How long should the process names be in the kernel? */
#define P_NAME_LEN	8

/* How many bytes should the circular buffer for kernel diagnostics. */
#define KMESS_BUF_SIZE   256   	

/* Maximum size in bytes for (port,value)-pairs vector to copy in. */
#define VDEVIO_BUF_SIZE   64

/* How many elements in vector of virtual copy requests. */
#define VCOPY_VEC_SIZE    16

/* How many IRQ hooks are there in total. */
#define NR_IRQ_HOOKS	  16

/* How many buffers for notification messages should there be? */
#define NR_NOTIFY_BUFS	  32

/* Buffer to gather randomness. How many entries before wrapping? */
#define RANDOM_ELEMENTS   32

/* Constants and macros for bit map manipulation. */
#define BITCHUNK_BITS   (sizeof(bitchunk_t) * CHAR_BIT)
#define BITMAP_CHUNKS(nr_bits) (((nr_bits)+BITCHUNK_BITS-1)/BITCHUNK_BITS)  
#define MAP_CHUNK(map,bit) (map)[((bit)/BITCHUNK_BITS)]
#define CHUNK_OFFSET(bit) ((bit)%BITCHUNK_BITS))
#define GET_BIT(map,bit) ( MAP_CHUNK(map,bit) & (1 << CHUNK_OFFSET(bit) )
#define SET_BIT(map,bit) ( MAP_CHUNK(map,bit) |= (1 << CHUNK_OFFSET(bit) )
#define UNSET_BIT(map,bit) ( MAP_CHUNK(map,bit) &= ~(1 << CHUNK_OFFSET(bit) )


#if (CHIP == INTEL)

/* Program stack words and masks. */
#define INIT_PSW      0x0200	/* initial psw */
#define INIT_TASK_PSW 0x1200	/* initial psw for tasks (with IOPL 1) */
#define TRACEBIT      0x0100	/* OR this with psw in proc[] for tracing */
#define SETPSW(rp, new)	/* permits only certain bits to be set */ \
	((rp)->p_reg.psw = (rp)->p_reg.psw & ~0xCD5 | (new) & 0xCD5)
#define IF_MASK 0x00000200
#define IOPL_MASK 0x003000

#if ENABLE_LOCK_TIMING
#define locktimestart(c, v) timer_start(c, v)
#define locktimeend(c) timer_end(c)
#else
#define locktimestart(c, v)
#define locktimeend(c)
#endif

/* Disable/Enable hardware interrupts. */
#define lock(c, v)	do { intr_disable(); locktimestart(c, v); } while(0)
#define unlock(c)	do { locktimeend(c); intr_enable(); } while(0)

/* Sizes of memory tables. The boot monitor distinguishes three memory areas, 
 * namely low mem below 1M, 1M-16M, and mem after 16M. More chunks are needed
 * for DOS MINIX.
 */
#define NR_MEMS            8	/* number of chunks of memory */

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific constants go here. */
#endif /* (CHIP == M68000) */

#if ENABLE_INT_TIMING
#define INT_TIMING_BITS		12
#define INT_TIMING_ELEMENTS	(1L << 12)
#endif
