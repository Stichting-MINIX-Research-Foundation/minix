
#ifndef _I386_TYPES_H
#define _I386_TYPES_H

#include <minix/sys_config.h>
#include "archconst.h"

typedef unsigned reg_t;         /* machine register */
typedef reg_t segdesc_t;

/* The stack frame layout is determined by the software, but for efficiency
 * it is laid out so the assembly code to use it is as simple as possible.
 * 80286 protected mode and all real modes use the same frame, built with
 * 16-bit registers.  Real mode lacks an automatic stack switch, so little
 * is lost by using the 286 frame for it.  The 386 frame differs only in
 * having 32-bit registers and more segment registers.  The same names are
 * used for the larger registers to avoid differences in the code.
 */
struct stackframe_s {           /* proc_ptr points here */
  u16_t gs;                     /* last item pushed by save */
  u16_t fs;                     /*  ^ */
  u16_t es;                     /*  | */
  u16_t ds;                     /*  | */
  reg_t di;			/* di through cx are not accessed in C */
  reg_t si;			/* order is to match pusha/popa */
  reg_t fp;			/* bp */
  reg_t st;			/* hole for another copy of sp */
  reg_t bx;                     /*  | */
  reg_t dx;                     /*  | */
  reg_t cx;                     /*  | */
  reg_t retreg;			/* ax and above are all pushed by save */
  reg_t retadr;			/* return address for assembly code save() */
  reg_t pc;			/*  ^  last item pushed by interrupt */
  reg_t cs;                     /*  | */
  reg_t psw;                    /*  | */
  reg_t sp;                     /*  | */
  reg_t ss;                     /* these are pushed by CPU during interrupt */
};

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
	struct segdesc_s p_ldt[2+NR_REMOTE_SEGS]; /* CS, DS and remote */
} segframe_t;

/* Page fault event. Stored in process table. Only valid if PAGEFAULT
 * set in p_rts_flags.
 */
struct pagefault
{
	u32_t   pf_virtual;     /* Address causing fault (CR2). */
	u32_t   pf_flags;       /* Pagefault flags on stack. */
};

#endif /* #ifndef _I386_TYPES_H */

