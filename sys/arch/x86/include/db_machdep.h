/*	$NetBSD: db_machdep.h,v 1.3 2011/04/30 16:58:35 christos Exp $	*/

#ifndef _X86_DB_MACHDEP_H_
#define _X86_DB_MACHDEP_H_

#define	INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

#define NONE		0
#define TRAP		1
#define SYSCALL		2
#define INTERRUPT	3
#define INTERRUPT_TSS	4
#define TRAP_TSS	5

#define MAXNARG		16

struct db_variable;

#ifdef __x86_64__
#define	tf_sp		tf_rsp
#define	tf_ip		tf_rip
#define	tf_bp		tf_rbp
#define	pcb_bp		pcb_rbp
#define	pcb_sp		pcb_rsp
#define	x86_frame	x86_64_frame
#else
#define	tf_sp		tf_esp
#define	tf_ip		tf_eip
#define	tf_bp		tf_ebp
#define	pcb_bp		pcb_ebp
#define	pcb_sp		pcb_esp
#define	x86_frame	i386_frame
#endif

int db_x86_regop(const struct db_variable *, db_expr_t *, int);
int db_numargs(long *);
int db_nextframe(long **, long **, long **, db_addr_t *, long *, int,
		 void (*) (const char *, ...));
db_sym_t db_frame_info(long *, db_addr_t, const char **, db_expr_t *,
                       int *, int *);

#endif /* _X86_DB_MACHDEP_H_ */
