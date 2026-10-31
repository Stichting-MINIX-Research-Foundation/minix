/*	$NetBSD: db_machdep.c,v 1.13 2022/06/27 23:36:48 riastradh Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: db_machdep.c,v 1.13 2022/06/27 23:36:48 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/syscall.h>

#include <machine/frame.h>
#include <machine/trap.h>
#include <machine/intrdefs.h>

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>
#include <ddb/db_user.h>
#include <ddb/db_proc.h>
#include <ddb/db_command.h>
#include <x86/db_machdep.h>

#define dbreg(xx) (long *)offsetof(db_regs_t, tf_ ## xx)

/*
 * Machine register set.
 */
const struct db_variable db_regs[] = {
	{ "ds",		dbreg(ds),     db_x86_regop, NULL },
	{ "es",		dbreg(es),     db_x86_regop, NULL },
	{ "fs",		dbreg(fs),     db_x86_regop, NULL },
	{ "gs",		dbreg(gs),     db_x86_regop, NULL },
	{ "rdi",	dbreg(rdi),    db_x86_regop, NULL },
	{ "rsi",	dbreg(rsi),    db_x86_regop, NULL },
	{ "rbp",	dbreg(rbp),    db_x86_regop, NULL },
	{ "rbx",	dbreg(rbx),    db_x86_regop, NULL },
	{ "rdx",	dbreg(rdx),    db_x86_regop, NULL },
	{ "rcx",	dbreg(rcx),    db_x86_regop, NULL },
	{ "rax",	dbreg(rax),    db_x86_regop, NULL },
	{ "r8",		dbreg(r8),     db_x86_regop, NULL },
	{ "r9",		dbreg(r9),     db_x86_regop, NULL },
	{ "r10",	dbreg(r10),    db_x86_regop, NULL },
	{ "r11",	dbreg(r11),    db_x86_regop, NULL },
	{ "r12",	dbreg(r12),    db_x86_regop, NULL },
	{ "r13",	dbreg(r13),    db_x86_regop, NULL },
	{ "r14",	dbreg(r14),    db_x86_regop, NULL },
	{ "r15",	dbreg(r15),    db_x86_regop, NULL },
	{ "rip",	dbreg(rip),    db_x86_regop, NULL },
	{ "cs",		dbreg(cs),     db_x86_regop, NULL },
	{ "rflags",	dbreg(rflags), db_x86_regop, NULL },
	{ "rsp",	dbreg(rsp),    db_x86_regop, NULL },
	{ "ss",		dbreg(ss),     db_x86_regop, NULL },
};
const struct db_variable * const db_eregs =
    db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Figure out how many arguments were passed into the frame at "fp".
 * We can probably figure out how many arguments where passed above
 * the first 6 (which are in registers), but since we can't
 * reliably determine the values currently, just return 0.
 */
int
db_numargs(long *retaddrp)
{
	return 0;
}

/*
 * Figure out the next frame up in the call stack.
 * For trap(), we print the address of the faulting instruction and
 *   proceed with the calling frame.  We return the ip that faulted.
 *   If the trap was caused by jumping through a bogus pointer, then
 *   the next line in the backtrace will list some random function as
 *   being called.  It should get the argument list correct, though.
 *   It might be possible to dig out from the next frame up the name
 *   of the function that faulted, but that could get hairy.
 */
int
db_nextframe(long **nextframe, long **retaddr, long **arg0, db_addr_t *ip,
    long *argp, int is_trap, void (*pr)(const char *, ...))
{
	struct trapframe *tf;
	struct x86_64_frame *fp;
	struct intrframe *ifp;
	int traptype, trapno, err, i;
	db_expr_t syscallno;

	switch (is_trap) {
	    case NONE:
		*ip = (db_addr_t)
			db_get_value((long)*retaddr, 8, false);
		fp = (struct x86_64_frame *)
			db_get_value((long)*nextframe, 8, false);
		if (fp == NULL)
			return 0;
		*nextframe = (long *)&fp->f_frame;
		*retaddr = (long *)&fp->f_retaddr;
		*arg0 = (long *)&fp->f_arg0;
		break;

	    case SYSCALL:
		tf = (struct trapframe *)argp;
		syscallno = db_get_value((long)&tf->tf_rax, 8, false);
		if (syscallno == SYS_syscall || syscallno == SYS___syscall) {
			syscallno = db_get_value((long)&tf->tf_rdi, 8, false);
			(*pr)("--- syscall (number %"DDB_EXPR_FMT"u"
			    " via SYS_syscall) ---\n",
			    syscallno);
		} else {
			(*pr)("--- syscall (number %"DDB_EXPR_FMT"u) ---\n",
			    syscallno);
		}
		return 0;

	    case TRAP:
	    case INTERRUPT:
	    default:

		/* The only argument to trap() is the trapframe. */
		tf = (struct trapframe *)argp;
		switch (is_trap) {
		case TRAP:
			(*pr)("--- trap (number %"DDB_EXPR_FMT"u) ---\n",
				db_get_value((long)&tf->tf_trapno, 8, false));
			break;
		case INTERRUPT:
			(*pr)("--- interrupt ---\n");
			break;
		}
		*ip = (db_addr_t)db_get_value((long)&tf->tf_rip, 8, false);
		fp = (struct x86_64_frame *)
			db_get_value((long)&tf->tf_rbp, 8, false);
		if (fp == NULL)
			return 0;
		if (((uintptr_t)fp & 7) != 0)
			return 0;
		*nextframe = (long *)&fp->f_frame;
		*retaddr = (long *)&fp->f_retaddr;
		*arg0 = (long *)&fp->f_arg0;
		break;
	}

	/*
	 * A bit of a hack. Since %rbp may be used in the stub code,
	 * walk the stack looking for a valid interrupt frame. Such
	 * a frame can be recognized by always having
	 * err 0 or IREENT_MAGIC and trapno T_ASTFLT.
	 */
	if (db_frame_info(*nextframe, (db_addr_t)*ip, NULL, NULL, &traptype,
	    NULL) != (db_sym_t)0
	    && traptype == INTERRUPT) {
		for (i = 0; i < 4; i++) {
			ifp = (struct intrframe *)(argp + i);
			err = db_get_value((long)&ifp->if_tf.tf_err,
			    sizeof(long), false);
			trapno = db_get_value((long)&ifp->if_tf.tf_trapno,
			    sizeof(long), false);
			if ((err == 0 || err == IREENT_MAGIC)
			    && trapno == T_ASTFLT) {
				*nextframe = (long *)ifp - 1;
				break;
			}
		}
		if (i == 4) {
			(*pr)("DDB lost frame for ");
			db_printsym(*ip, DB_STGY_ANY, pr);
			(*pr)(", trying %p\n",argp);
			*nextframe = argp;
		}
	}
	return 1;
}

db_sym_t
db_frame_info(long *frame, db_addr_t callpc, const char **namep,
    db_expr_t *offp, int *is_trap, int *nargp)
{
	db_expr_t offset;
	db_sym_t sym;
	int narg;
	const char *name;

	sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
	if (sym != 0 && offset == 0) {
		sym = db_search_symbol(callpc - 1, DB_STGY_ANY, &offset);
		offset++;
	}
	db_symbol_values(sym, &name, NULL);
	if (sym == (db_sym_t)0)
		return (db_sym_t)0;

	*is_trap = NONE;
	narg = 0;

	if (INKERNEL((long)frame) && name) {
		/*
		 * XXX traps should be based off of the Xtrap*
		 * locations rather than on trap, since some traps
		 * (e.g., npxdna) don't go through trap()
		 */
		if (!strcmp(name, "trap")) {
			*is_trap = TRAP;
			narg = 0;
		} else if (!strcmp(name, "syscall") ||
		    !strcmp(name, "handle_syscall")) {
			*is_trap = SYSCALL;
			narg = 0;
		} else if (name[0] == 'X') {
			if (!strncmp(name, "Xintr", 5) ||
			    !strncmp(name, "Xhandle", 7) ||
			    !strncmp(name, "Xresume", 7) ||
			    !strncmp(name, "Xstray", 6) ||
			    !strncmp(name, "Xhold", 5) ||
			    !strncmp(name, "Xrecurse", 8) ||
			    !strcmp(name, "Xdoreti") ||
			    !strncmp(name, "Xsoft", 5)) {
				*is_trap = INTERRUPT;
				narg = 0;
			}
		}
	}

	if (offp != NULL)
		*offp = offset;
	if (nargp != NULL)
		*nargp = narg;
	if (namep != NULL)
		*namep = name;
	return sym;
}
