#ifndef STACK_FRAME_H
#define STACK_FRAME_H

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
struct stackframe_s {
	u16_t gs;                     /* last item pushed by save */
	u16_t fs;                     /*  ^ */
	u16_t es;                     /*  | */
	u16_t ds;                     /*  | */
	reg_t di;                     /* di through cx are not accessed in C */
	reg_t si;                     /* order is to match pusha/popa */
	reg_t fp;                     /* bp */
	reg_t st;                     /* hole for another copy of sp */
	reg_t bx;                     /*  | */
	reg_t dx;                     /*  | */
	reg_t cx;                     /*  | */
	reg_t retreg;                 /* ax and above are all pushed by save */
	reg_t retadr;                 /* return address for assembly code save() */
	reg_t pc;                     /*  ^  last item pushed by interrupt */
	reg_t cs;                     /*  | */
	reg_t psw;                    /*  | */
	reg_t sp;                     /*  | */
	reg_t ss;                     /* these are pushed by CPU during interrupt */
};

#endif /* #ifndef STACK_FRAME_H */
