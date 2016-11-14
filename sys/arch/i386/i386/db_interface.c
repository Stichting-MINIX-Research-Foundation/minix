/*	$NetBSD: db_interface.c,v 1.71 2014/02/28 10:16:51 skrll Exp $	*/

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
 *
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: db_interface.c,v 1.71 2014/02/28 10:16:51 skrll Exp $");

#include "opt_ddb.h"
#include "opt_multiprocessor.h"

#include "ioapic.h"
#include "lapic.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/cpu.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <machine/cpufunc.h>
#include <machine/db_machdep.h>
#include <machine/cpuvar.h>
#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif
#if NLAPIC > 0
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_extern.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#include <ddb/ddbvar.h>

extern const char *const trap_type[];
extern int trap_types;

int	db_active = 0;
db_regs_t ddb_regs;	/* register state */

void db_mach_cpu (db_expr_t, bool, db_expr_t, const char *);

const struct db_command db_machine_command_table[] = {
#ifdef MULTIPROCESSOR
	{ DDB_ADD_CMD("cpu",	db_mach_cpu,	0,
	  "switch to another cpu", "cpu-no", NULL) },
#endif
		
	{ DDB_ADD_CMD(NULL, NULL, 0,  NULL,NULL,NULL) },
};

void kdbprinttrap(int, int);
#ifdef MULTIPROCESSOR
extern void ddb_ipi(int, struct trapframe);
extern void ddb_ipi_tss(struct i386tss *);
static void ddb_suspend(struct trapframe *);
#ifndef XEN
int ddb_vec;
#endif /* XEN */
static bool ddb_mp_online;
#endif

db_regs_t *ddb_regp = 0;

#define NOCPU -1

int ddb_cpu = NOCPU;

typedef void (vector)(void);
extern vector Xintrddbipi;

void
db_machine_init(void)
{

#ifdef MULTIPROCESSOR
#ifndef XEN
	ddb_vec = idt_vec_alloc(0xf0, 0xff);
	idt_vec_set(ddb_vec, &Xintrddbipi);
#else
	/* Initialised as part of xen_ipi_init() */
#endif /* XEN */
#endif
}

#ifdef MULTIPROCESSOR

__cpu_simple_lock_t db_lock;

static int
db_suspend_others(void)
{
	int cpu_me = cpu_number();
	int win;

#ifndef XEN
	if (ddb_vec == 0)
		return 1;
#endif /* XEN */

	__cpu_simple_lock(&db_lock);
	if (ddb_cpu == NOCPU)
		ddb_cpu = cpu_me;
	win = (ddb_cpu == cpu_me);
	__cpu_simple_unlock(&db_lock);
	if (win) {
#ifdef XEN
		xen_broadcast_ipi(XEN_IPI_DDB);
#else
		x86_ipi(ddb_vec, LAPIC_DEST_ALLEXCL, LAPIC_DLMODE_FIXED);
#endif /* XEN */
	}
	ddb_mp_online = x86_mp_online;
	x86_mp_online = false;
	return win;
}

static void
db_resume_others(void)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	x86_mp_online = ddb_mp_online;
	__cpu_simple_lock(&db_lock);
	ddb_cpu = NOCPU;
	__cpu_simple_unlock(&db_lock);

	for (CPU_INFO_FOREACH(cii, ci)) {
		if (ci->ci_flags & CPUF_PAUSE)
			atomic_and_32(&ci->ci_flags, ~CPUF_PAUSE);
	}
}

#endif

/*
 * Print trap reason.
 */
void
kdbprinttrap(int type, int code)
{
	db_printf("kernel: %s trap ", (type & T_USER) ? "user" : "supervisor");
	type &= ~T_USER;
	if (type >= trap_types || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", trap_type[type]);
	db_printf(", code=%x\n", code);
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(int type, int code, db_regs_t *regs)
{
	int s, flags;
	db_regs_t dbreg;

	flags = regs->tf_err & TC_FLAGMASK;
	regs->tf_err &= ~TC_FLAGMASK;

	switch (type) {
	case T_NMI:	/* NMI */
		printf("NMI ... going to debugger\n");
		/*FALLTHROUGH*/
	case T_BPTFLT:	/* breakpoint */
	case T_TRCTRAP:	/* single_step */
	case -1:	/* keyboard interrupt */
		break;
	default:
		if (!db_onpanic && db_recover == 0)
			return (0);

		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}

#ifdef MULTIPROCESSOR
	if (!db_suspend_others()) {
		ddb_suspend(regs);
	} else {
	curcpu()->ci_ddb_regs = &dbreg;
	ddb_regp = &dbreg;
#endif
	/* XXX Should switch to kdb's own stack here. */
	ddb_regs = *regs;
	if (!(flags & TC_TSS) && KERNELMODE(regs->tf_cs, regs->tf_eflags)) {
		/*
		 * Kernel mode - esp and ss not saved
		 */
		ddb_regs.tf_esp = (int)&regs->tf_esp;	/* kernel stack pointer */
		ddb_regs.tf_ss = x86_getss();
	}

	ddb_regs.tf_cs &= 0xffff;
	ddb_regs.tf_ds &= 0xffff;
	ddb_regs.tf_es &= 0xffff;
	ddb_regs.tf_fs &= 0xffff;
	ddb_regs.tf_gs &= 0xffff;
	ddb_regs.tf_ss &= 0xffff;
	s = splhigh();
	db_active++;
	cnpollc(true);
	db_trap(type, code);
	cnpollc(false);
	db_active--;
	splx(s);
#ifdef MULTIPROCESSOR
	db_resume_others();
	}
#endif
	ddb_regp = &dbreg;

	regs->tf_gs     = ddb_regs.tf_gs;
	regs->tf_fs     = ddb_regs.tf_fs;
	regs->tf_es     = ddb_regs.tf_es;
	regs->tf_ds     = ddb_regs.tf_ds;
	regs->tf_edi    = ddb_regs.tf_edi;
	regs->tf_esi    = ddb_regs.tf_esi;
	regs->tf_ebp    = ddb_regs.tf_ebp;
	regs->tf_ebx    = ddb_regs.tf_ebx;
	regs->tf_edx    = ddb_regs.tf_edx;
	regs->tf_ecx    = ddb_regs.tf_ecx;
	regs->tf_eax    = ddb_regs.tf_eax;
	regs->tf_eip    = ddb_regs.tf_eip;
	regs->tf_cs     = ddb_regs.tf_cs;
	regs->tf_eflags = ddb_regs.tf_eflags;
	if (!(flags & TC_TSS) && !KERNELMODE(regs->tf_cs, regs->tf_eflags)) {
		/* ring transit - saved esp and ss valid */
		regs->tf_esp    = ddb_regs.tf_esp;
		regs->tf_ss     = ddb_regs.tf_ss;
	}

#ifdef TRAPLOG
	wrmsr(MSR_DEBUGCTLMSR, 0x1);
#endif

	return (1);
}

void
cpu_Debugger(void)
{
	breakpoint();
}

#ifdef MULTIPROCESSOR

/*
 * Called when we receive a debugger IPI (inter-processor interrupt).
 * As with trap() in trap.c, this function is called from an assembly
 * language IDT gate entry routine which prepares a suitable stack frame,
 * and restores this frame after the exception has been processed. Note
 * that the effect is as if the arguments were passed call by reference.
 */
void
ddb_ipi(int cpl, struct trapframe frame)
{

	ddb_suspend(&frame);
}

void
ddb_ipi_tss(struct i386tss *tss)
{
	struct trapframe tf;

	tf.tf_gs = tss->tss_gs;
	tf.tf_fs = tss->tss_fs;
	tf.tf_es = tss->__tss_es;
	tf.tf_ds = tss->__tss_ds;
	tf.tf_edi = tss->__tss_edi;
	tf.tf_esi = tss->__tss_esi;
	tf.tf_ebp = tss->tss_ebp;
	tf.tf_ebx = tss->__tss_ebx;
	tf.tf_edx = tss->__tss_edx;
	tf.tf_ecx = tss->__tss_ecx;
	tf.tf_eax = tss->__tss_eax;
	tf.tf_trapno = 0;
	tf.tf_err = TC_TSS;
	tf.tf_eip = tss->__tss_eip;
	tf.tf_cs = tss->__tss_cs;
	tf.tf_eflags = tss->__tss_eflags;
	tf.tf_esp = tss->tss_esp;
	tf.tf_ss = tss->__tss_ss;

	ddb_suspend(&tf);
}

static void
ddb_suspend(struct trapframe *frame)
{
	volatile struct cpu_info *ci = curcpu();
	db_regs_t regs;
	int flags;

	regs = *frame;
	flags = regs.tf_err & TC_FLAGMASK;
	regs.tf_err &= ~TC_FLAGMASK;
	if (!(flags & TC_TSS) && KERNELMODE(regs.tf_cs, regs.tf_eflags)) {
		/*
		 * Kernel mode - esp and ss not saved
		 */
		regs.tf_esp = (int)&frame->tf_esp; /* kernel stack pointer */
		regs.tf_ss = x86_getss();
	}

	ci->ci_ddb_regs = &regs;

	atomic_or_32(&ci->ci_flags, CPUF_PAUSE);
	while (ci->ci_flags & CPUF_PAUSE)
		;
	ci->ci_ddb_regs = 0;
	tlbflushg();
}


extern void cpu_debug_dump(void); /* XXX */

void
db_mach_cpu(
	db_expr_t	addr,
	bool		have_addr,
	db_expr_t	count,
	const char *	modif)
{
	struct cpu_info *ci;
	if (!have_addr) {
		cpu_debug_dump();
		return;
	}

	if (addr < 0) {
		db_printf("%ld: CPU out of range\n", addr);
		return;
	}
	ci = cpu_lookup(addr);
	if (ci == NULL) {
		db_printf("CPU %ld not configured\n", addr);
		return;
	}
	if (ci != curcpu()) {
		if (!(ci->ci_flags & CPUF_PAUSE)) {
			db_printf("CPU %ld not paused\n", addr);
			return;
		}
	}
	if (ci->ci_ddb_regs == 0) {
		db_printf("CPU %ld has no saved regs\n", addr);
		return;
	}
	db_printf("using CPU %ld", addr);
	ddb_regp = ci->ci_ddb_regs;
}


#endif
