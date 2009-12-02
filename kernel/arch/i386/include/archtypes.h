
#ifndef _I386_TYPES_H
#define _I386_TYPES_H

#include <minix/sys_config.h>
#include "archconst.h"
#include <sys/stackframe.h>
#include <sys/fpu.h>

struct segdesc_s {		/* segment descriptor for protected mode */
  u16_t limit_low;
  u16_t base_low;
  u8_t base_middle;
  u8_t access;		/* |P|DL|1|X|E|R|A| */
  u8_t granularity;	/* |G|X|0|A|LIMT| */
  u8_t base_high;
};

#define LDT_SIZE (2 + NR_REMOTE_SEGS)   /* CS, DS and remote segments */

/* Fixed local descriptors. */
#define CS_LDT_INDEX         0  /* process CS */
#define DS_LDT_INDEX         1  /* process DS=ES=FS=GS=SS */
#define EXTRA_LDT_INDEX      2  /* first of the extra LDT entries */

typedef struct segframe {
	reg_t p_ldt_sel;    /* selector in gdt with ldt base and limit */
	reg_t	p_cr3;		/* page table root */
	struct segdesc_s p_ldt[LDT_SIZE]; /* CS, DS and remote */
} segframe_t;

/* Page fault event. Stored in process table. Only valid if PAGEFAULT
 * set in p_rts_flags.
 */
struct pagefault
{
	u32_t   pf_virtual;     /* Address causing fault (CR2). */
	u32_t   pf_flags;       /* Pagefault flags on stack. */
};


/* fpu_state_s is used in kernel proc table.
 * Any changes in this structure requires changes in sconst.h,
 * since this structure is used in proc structure. */
struct fpu_state_s {
	union fpu_state_u *fpu_save_area_p; /* 16-aligned fpu_save_area */
	/* fpu_image includes 512 bytes of image itself and
	 * additional 15 bytes required for manual 16-byte alignment. */
	char fpu_image[527];
};

#define INMEMORY(p) (!p->p_seg.p_cr3 || ptproc == p)

#endif /* #ifndef _I386_TYPES_H */

