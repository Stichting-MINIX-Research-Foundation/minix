/*	$NetBSD: db_machdep.c,v 1.5 2014/01/11 17:11:50 christos Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: db_machdep.c,v 1.5 2014/01/11 17:11:50 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#ifndef _KERNEL
#include <stdbool.h>
#endif

#include <machine/frame.h>
#include <machine/trap.h>
#include <machine/intrdefs.h>
#include <machine/cpu.h>

#include <uvm/uvm_prot.h>
/* We need to include both for ddb and crash(8).  */
#include <uvm/uvm_pmap.h>
#include <machine/pmap.h>

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>
#include <ddb/db_user.h>
#include <ddb/db_proc.h>
#include <ddb/db_command.h>
#include <ddb/db_cpu.h>
#include <x86/db_machdep.h>

#define dbreg(xx) (long *)offsetof(db_regs_t, tf_ ## xx)

const struct db_variable db_regs[] = {
	{ "ds",		dbreg(ds),     db_x86_regop, NULL },
	{ "es",		dbreg(es),     db_x86_regop, NULL },
	{ "fs",		dbreg(fs),     db_x86_regop, NULL },
	{ "gs",		dbreg(gs),     db_x86_regop, NULL },
	{ "edi",	dbreg(edi),    db_x86_regop, NULL },
	{ "esi",	dbreg(esi),    db_x86_regop, NULL },
	{ "ebp",	dbreg(ebp),    db_x86_regop, NULL },
	{ "ebx",	dbreg(ebx),    db_x86_regop, NULL },
	{ "edx",	dbreg(edx),    db_x86_regop, NULL },
	{ "ecx",	dbreg(ecx),    db_x86_regop, NULL },
	{ "eax",	dbreg(eax),    db_x86_regop, NULL },
	{ "eip",	dbreg(eip),    db_x86_regop, NULL },
	{ "cs",		dbreg(cs),     db_x86_regop, NULL },
	{ "eflags",	dbreg(eflags), db_x86_regop, NULL },
	{ "esp",	dbreg(esp),    db_x86_regop, NULL },
	{ "ss",		dbreg(ss),     db_x86_regop, NULL },
};
const struct db_variable * const db_eregs =
    db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
int
db_numargs(long *retaddrp)
{
	int	*argp;
	int	inst;
	int	args;
	extern char	etext[];

	argp = (int *)db_get_value((int)retaddrp, 4, false);
	if (argp < (int *)VM_MIN_KERNEL_ADDRESS || argp > (int *)etext) {
		args = 10;
	} else {
		inst = db_get_value((int)argp, 4, false);
		if ((inst & 0xff) == 0x59)	/* popl %ecx */
			args = 1;
		else if ((inst & 0xffff) == 0xc483)	/* addl %n, %esp */
			args = ((inst >> 16) & 0xff) / 4;
		else
			args = 10;
	}
	return (args);
}

db_sym_t
db_frame_info(long *frame, db_addr_t callpc, const char **namep, db_expr_t *offp,
	      int *is_trap, int *nargp)
{
	db_expr_t	offset;
	db_sym_t	sym;
	int narg;
	const char *name;

	sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
	db_symbol_values(sym, &name, NULL);
	if (sym == (db_sym_t)0)
		return (db_sym_t)0;

	*is_trap = NONE;
	narg = MAXNARG;

	if (INKERNEL((int)frame) && name) {
		/*
		 * XXX traps should be based off of the Xtrap*
		 * locations rather than on trap, since some traps
		 * (e.g., npxdna) don't go through trap()
		 */
#ifdef __ELF__
		if (!strcmp(name, "trap_tss")) {
			*is_trap = TRAP_TSS;
			narg = 0;
		} else if (!strcmp(name, "trap")) {
			*is_trap = TRAP;
			narg = 0;
		} else if (!strcmp(name, "syscall")) {
			*is_trap = SYSCALL;
			narg = 0;
		} else if (name[0] == 'X') {
			if (!strncmp(name, "Xintr", 5) ||
			    !strncmp(name, "Xresume", 7) ||
			    !strncmp(name, "Xstray", 6) ||
			    !strncmp(name, "Xhold", 5) ||
			    !strncmp(name, "Xrecurse", 8) ||
			    !strcmp(name, "Xdoreti")) {
				*is_trap = INTERRUPT;
				narg = 0;
			} else if (!strcmp(name, "Xsoftintr")) {
				*is_trap = SOFTINTR;
				narg = 0;
			} else if (!strncmp(name, "Xtss_", 5)) {
				*is_trap = INTERRUPT_TSS;
				narg = 0;
			}
		}
#else
		if (!strcmp(name, "_trap_tss")) {
			*is_trap = TRAP_TSS;
			narg = 0;
		} else if (!strcmp(name, "_trap")) {
			*is_trap = TRAP;
			narg = 0;
		} else if (!strcmp(name, "_syscall")) {
			*is_trap = SYSCALL;
			narg = 0;
		} else if (name[0] == '_' && name[1] == 'X') {
			if (!strncmp(name, "_Xintr", 6) ||
			    !strncmp(name, "_Xresume", 8) ||
			    !strncmp(name, "_Xstray", 7) ||
			    !strncmp(name, "_Xhold", 6) ||
			    !strncmp(name, "_Xrecurse", 9) ||
			    !strcmp(name, "_Xdoreti")) {
				*is_trap = INTERRUPT;
				narg = 0;
			} else if (!strcmp(name, "_Xsoftintr")) {
				*is_trap = SOFTINTR;
				narg = 0;
			} else if (!strncmp(name, "_Xtss_", 6)) {
				*is_trap = INTERRUPT_TSS;
				narg = 0;
			}
		}
#endif /* __ELF__ */
	}

	if (offp != NULL)
		*offp = offset;
	if (nargp != NULL)
		*nargp = narg;
	if (namep != NULL)
		*namep = name;
	return sym;
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
db_nextframe(
    long **nextframe,	/* IN/OUT */
    long **retaddr,	/* IN/OUT */
    long **arg0,		/* OUT */
    db_addr_t *ip,	/* OUT */
    long *argp,		/* IN */
    int is_trap, void (*pr)(const char *, ...))
{
	static struct trapframe tf;
	static struct i386tss tss;
	struct i386_frame *fp;
	int traptype;
	uintptr_t ptr;

	switch (is_trap) {
	    case NONE:
		*ip = (db_addr_t)
			db_get_value((int)*retaddr, 4, false);
		fp = (struct i386_frame *)
			db_get_value((int)*nextframe, 4, false);
		if (fp == NULL)
			return 0;
		*nextframe = (long *)&fp->f_frame;
		*retaddr = (long *)&fp->f_retaddr;
		*arg0 = (long *)&fp->f_arg0;
		break;

	    case TRAP_TSS:
	    case INTERRUPT_TSS:
		ptr = db_get_value((int)argp, 4, false);
		db_read_bytes((db_addr_t)ptr, sizeof(tss), (char *)&tss);
		*ip = tss.__tss_eip;
		fp = (struct i386_frame *)tss.tss_ebp;
		if (fp == NULL)
			return 0;
		*nextframe = (long *)&fp->f_frame;
		*retaddr = (long *)&fp->f_retaddr;
		*arg0 = (long *)&fp->f_arg0;
		if (is_trap == INTERRUPT_TSS)
			(*pr)("--- interrupt via task gate ---\n");
		else
			(*pr)("--- trap via task gate ---\n");
		break;

	    case TRAP:
	    case SYSCALL:
	    case INTERRUPT:
	    case SOFTINTR:
	    default:
		/* The only argument to trap() or syscall() is the trapframe. */
		switch (is_trap) {
		case TRAP:
			ptr = db_get_value((int)argp, 4, false);
			db_read_bytes((db_addr_t)ptr, sizeof(tf), (char *)&tf);
			(*pr)("--- trap (number %d) ---\n", tf.tf_trapno);
			break;
		case SYSCALL:
			ptr = db_get_value((int)argp, 4, false);
			db_read_bytes((db_addr_t)ptr, sizeof(tf), (char *)&tf);
			(*pr)("--- syscall (number %d) ---\n", tf.tf_eax);
			break;
		case INTERRUPT:
			(*pr)("--- interrupt ---\n");
			/*
			 * see the "XXX -1 here is a hack" comment below.
			 */
			db_read_bytes((db_addr_t)argp, sizeof(tf), (char *)&tf);
			break;
		case SOFTINTR:
			(*pr)("--- softint ---\n");
			tf.tf_eip = 0;
			tf.tf_ebp = 0;
			break;
		}
		*ip = (db_addr_t)tf.tf_eip;
		fp = (struct i386_frame *)tf.tf_ebp;
		if (fp == NULL)
			return 0;
		*nextframe = (long *)&fp->f_frame;
		*retaddr = (long *)&fp->f_retaddr;
		*arg0 = (long *)&fp->f_arg0;
		break;
	}

	/*
	 * A bit of a hack. Since %ebp may be used in the stub code,
	 * walk the stack looking for a valid interrupt frame. Such
	 * a frame can be recognized by always having
	 * err 0 or IREENT_MAGIC and trapno T_ASTFLT.
	 */
	if (db_frame_info(*nextframe, (db_addr_t)*ip, NULL, NULL, &traptype,
	    NULL) != (db_sym_t)0
	    && traptype == INTERRUPT) {
		struct intrframe *ifp;
		int trapno;
		int err;

		/*
		 * 2nd argument of interrupt handlers is a pointer to intrframe.
		 */
		ifp = (struct intrframe *)
		    db_get_value((db_addr_t)(argp + 1), sizeof(ifp), false);
		/*
		 * check if it's a valid intrframe.
		 */
		err = db_get_value((db_addr_t)&ifp->__if_err,
		    sizeof(ifp->__if_err), false);
		trapno = db_get_value((db_addr_t)&ifp->__if_trapno,
		    sizeof(ifp->__if_trapno), false);
		if ((err == 0 || err == IREENT_MAGIC) && trapno == T_ASTFLT) {
			/*
			 * found seemingly valid intrframe.
			 *
			 * XXX -1 here is a hack.
			 * for the next frame, we will be called with
			 * argp = *nextframe + 2.  (long *)if - 1 + 2 = &tf.
			 */
			*nextframe = (long *)ifp - 1;
		} else {
			(*pr)("DDB lost frame for ");
			db_printsym(*ip, DB_STGY_ANY, pr);
			(*pr)(", trying %p\n",argp);
			*nextframe = argp;
		}
	}
	return 1;
}

bool
db_intrstack_p(const void *vp)
{
	struct cpu_info *ci;
	const char *cp;

	for (ci = db_cpu_first(); ci != NULL; ci = db_cpu_next(ci)) {
		db_read_bytes((db_addr_t)&ci->ci_intrstack, sizeof(cp),
		    (char *)&cp);
		if (cp == NULL) {
			continue;
		}
		if ((cp - INTRSTACKSIZE + 4) <= (const char *)vp &&
		    (const char *)vp <= cp) {
			return true;
		}
	}
	return false;
}
