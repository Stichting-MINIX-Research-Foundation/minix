#ifndef STACK_FRAME_H
#define STACK_FRAME_H

#include <sys/types.h>

typedef uint64_t reg_t;		/* machine register */
typedef reg_t segdesc_t;

/*
 * AMD64 process register save area.
 *
 * Layout rules:
 *  - Segment slots kept for TLS (gs/fs) and historical compatibility.
 *  - r8-r11 are caller-saved scratch registers, saved on every kernel entry.
 *  - r12-r15 are callee-saved; saved only at process context switch in
 *    switch_to_user(), not on every IPC/interrupt entry.
 *  - The pc/cs/psw/sp/ss slots match the CPU-pushed long-mode interrupt frame
 *    so SAVE_TRAP_CTX can copy them in order.
 */
struct stackframe_s {
	/* segment registers — gs/fs used for TLS, es/ds vestigial in flat mode */
	u16_t gs;
	u16_t fs;
	u16_t es;
	u16_t ds;
	/* scratch registers — saved on every kernel entry (IPC, interrupt) */
	reg_t r8;
	reg_t r9;
	reg_t r10;	/* SYSCALL path: carries user RSP on entry (set by userspace) */
	reg_t r11;	/* SYSCALL path: CPU saves RFLAGS here; kernel moves to psw */
	/* classical general-purpose registers */
	reg_t di;	/* rdi: IPC arg 1 (call_nr) */
	reg_t si;	/* rsi: IPC arg 2 (endpoint) */
	reg_t fp;	/* rbp */
	reg_t bx;	/* rbx: IPC arg 3 (msg_ptr) ... wait, rdx is arg 3 */
	reg_t dx;	/* rdx: IPC arg 3 (msg_ptr) */
	reg_t cx;	/* rcx: SYSCALL saves return RIP here; kernel moves to pc */
	reg_t retreg;	/* rax: function return value / trap return value */
	/* CPU-pushed interrupt frame (matches long-mode exception frame layout) */
	reg_t pc;	/* rip */
	reg_t cs;
	reg_t psw;	/* rflags */
	reg_t sp;	/* rsp */
	reg_t ss;
	/* callee-saved registers — saved only at context switch */
	reg_t r12;
	reg_t r13;
	reg_t r14;
	reg_t r15;
};

#endif /* #ifndef STACK_FRAME_H */
