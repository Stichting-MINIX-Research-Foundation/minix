/*	$NetBSD: machdep.c,v 1.754 2015/04/24 00:04:04 khorben Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2000, 2004, 2006, 2008, 2009
 *     The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility NASA Ames Research Center, by Julio M. Merino Vidal,
 * and by Andrew Doran.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.754 2015/04/24 00:04:04 khorben Exp $");

#include "opt_beep.h"
#include "opt_compat_ibcs2.h"
#include "opt_compat_freebsd.h"
#include "opt_compat_netbsd.h"
#include "opt_compat_svr4.h"
#include "opt_cpureset_delay.h"
#include "opt_ddb.h"
#include "opt_ipkdb.h"
#include "opt_kgdb.h"
#include "opt_mtrr.h"
#include "opt_modular.h"
#include "opt_multiboot.h"
#include "opt_multiprocessor.h"
#include "opt_physmem.h"
#include "opt_realmem.h"
#include "opt_user_ldt.h"
#include "opt_vm86.h"
#include "opt_xen.h"
#include "isa.h"
#include "pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/ucontext.h>
#include <sys/ras.h>
#include <sys/ksyms.h>
#include <sys/device.h>

#ifdef IPKDB
#include <ipkdb/ipkdb.h>
#endif

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <dev/cons.h>
#include <dev/mm.h>

#include <uvm/uvm.h>
#include <uvm/uvm_page.h>

#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/gdt.h>
#include <machine/intr.h>
#include <machine/kcore.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/bootinfo.h>
#include <machine/mtrr.h>
#include <x86/x86/tsc.h>

#include <x86/fpu.h>
#include <x86/machdep.h>

#include <machine/multiboot.h>
#ifdef XEN
#include <xen/evtchn.h>
#include <xen/xen.h>
#include <xen/hypervisor.h>

/* #define	XENDEBUG */
/* #define	XENDEBUG_LOW */

#ifdef XENDEBUG
#define	XENPRINTF(x) printf x
#define	XENPRINTK(x) printk x
#else
#define	XENPRINTF(x)
#define	XENPRINTK(x)
#endif
#define	PRINTK(x) printf x
#endif /* XEN */

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>
#include <dev/ic/i8042reg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#ifdef VM86
#include <machine/vm86.h>
#endif

#include "acpica.h"
#include "bioscall.h"

#if NBIOSCALL > 0
#include <machine/bioscall.h>
#endif

#if NACPICA > 0
#include <dev/acpi/acpivar.h>
#define ACPI_MACHDEP_PRIVATE
#include <machine/acpi_machdep.h>
#endif

#include "isa.h"
#include "isadma.h"
#include "ksyms.h"

#include "cardbus.h"
#if NCARDBUS > 0
/* For rbus_min_start hint. */
#include <sys/bus.h>
#include <dev/cardbus/rbus.h>
#include <machine/rbus_machdep.h>
#endif

#include "mca.h"
#if NMCA > 0
#include <machine/mca_machdep.h>	/* for mca_busprobe() */
#endif

#ifdef MULTIPROCESSOR		/* XXX */
#include <machine/mpbiosvar.h>	/* XXX */
#endif				/* XXX */

/* the following is used externally (sysctl_hw) */
char machine[] = "i386";		/* CPU "architecture" */
char machine_arch[] = "i386";		/* machine == machine_arch */

extern struct bi_devmatch *x86_alldisks;
extern int x86_ndisks;

#ifdef CPURESET_DELAY
int	cpureset_delay = CPURESET_DELAY;
#else
int     cpureset_delay = 2000; /* default to 2s */
#endif

#ifdef MTRR
struct mtrr_funcs *mtrr_funcs;
#endif

int	cpu_class;
int	use_pae;
int	i386_fpu_present = 1;
int	i386_fpu_fdivbug;

int	i386_use_fxsave;
int	i386_has_sse;
int	i386_has_sse2;

vaddr_t	msgbuf_vaddr;
struct {
	paddr_t paddr;
	psize_t sz;
} msgbuf_p_seg[VM_PHYSSEG_MAX];
unsigned int msgbuf_p_cnt = 0;

vaddr_t	idt_vaddr;
paddr_t	idt_paddr;
vaddr_t	pentium_idt_vaddr;

struct vm_map *phys_map = NULL;

extern	paddr_t avail_start, avail_end;
#ifdef XEN
extern paddr_t pmap_pa_start, pmap_pa_end;
void hypervisor_callback(void);
void failsafe_callback(void);
#endif

#ifdef XEN
void (*delay_func)(unsigned int) = xen_delay;
void (*initclock_func)(void) = xen_initclocks;
#else
void (*delay_func)(unsigned int) = i8254_delay;
void (*initclock_func)(void) = i8254_initclocks;
#endif


/*
 * Size of memory segments, before any memory is stolen.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int	mem_cluster_cnt = 0;

void	init386(paddr_t);
void	initgdt(union descriptor *);

extern int time_adjusted;

int *esym;
int *eblob;
extern int boothowto;

#ifndef XEN

/* Base memory reported by BIOS. */
#ifndef REALBASEMEM
int	biosbasemem = 0;
#else
int	biosbasemem = REALBASEMEM;
#endif

/* Extended memory reported by BIOS. */
#ifndef REALEXTMEM
int	biosextmem = 0;
#else
int	biosextmem = REALEXTMEM;
#endif

/* Set if any boot-loader set biosbasemem/biosextmem. */
int	biosmem_implicit;

/* Representation of the bootinfo structure constructed by a NetBSD native
 * boot loader.  Only be used by native_loader(). */
struct bootinfo_source {
	uint32_t bs_naddrs;
	void *bs_addrs[1]; /* Actually longer. */
};

/* Only called by locore.h; no need to be in a header file. */
void	native_loader(int, int, struct bootinfo_source *, paddr_t, int, int);

/*
 * Called as one of the very first things during system startup (just after
 * the boot loader gave control to the kernel image), this routine is in
 * charge of retrieving the parameters passed in by the boot loader and
 * storing them in the appropriate kernel variables.
 *
 * WARNING: Because the kernel has not yet relocated itself to KERNBASE,
 * special care has to be taken when accessing memory because absolute
 * addresses (referring to kernel symbols) do not work.  So:
 *
 *     1) Avoid jumps to absolute addresses (such as gotos and switches).
 *     2) To access global variables use their physical address, which
 *        can be obtained using the RELOC macro.
 */
void
native_loader(int bl_boothowto, int bl_bootdev,
    struct bootinfo_source *bl_bootinfo, paddr_t bl_esym,
    int bl_biosextmem, int bl_biosbasemem)
{
#define RELOC(type, x) ((type)((vaddr_t)(x) - KERNBASE))

	*RELOC(int *, &boothowto) = bl_boothowto;

#ifdef COMPAT_OLDBOOT
	/*
	 * Pre-1.3 boot loaders gave the boot device as a parameter
	 * (instead of a bootinfo entry).
	 */
	*RELOC(int *, &bootdev) = bl_bootdev;
#endif

	/*
	 * The boot loader provides a physical, non-relocated address
	 * for the symbols table's end.  We need to convert it to a
	 * virtual address.
	 */
	if (bl_esym != 0)
		*RELOC(int **, &esym) = (int *)((vaddr_t)bl_esym + KERNBASE);
	else
		*RELOC(int **, &esym) = 0;

	/*
	 * Copy bootinfo entries (if any) from the boot loader's
	 * representation to the kernel's bootinfo space.
	 */
	if (bl_bootinfo != NULL) {
		size_t i;
		uint8_t *data;
		struct bootinfo *bidest;
		struct btinfo_modulelist *bi;

		bidest = RELOC(struct bootinfo *, &bootinfo);

		data = &bidest->bi_data[0];

		for (i = 0; i < bl_bootinfo->bs_naddrs; i++) {
			struct btinfo_common *bc;

			bc = bl_bootinfo->bs_addrs[i];

			if ((data + bc->len) >
			    (&bidest->bi_data[0] + BOOTINFO_MAXSIZE))
				break;

			memcpy(data, bc, bc->len);
			/*
			 * If any modules were loaded, record where they
			 * end.  We'll need to skip over them.
			 */
			bi = (struct btinfo_modulelist *)data;
			if (bi->common.type == BTINFO_MODULELIST) {
				*RELOC(int **, &eblob) =
				    (int *)(bi->endpa + KERNBASE);
			}
			data += bc->len;
		}
		bidest->bi_nentries = i;
	}

	/*
	 * Configure biosbasemem and biosextmem only if they were not
	 * explicitly given during the kernel's build.
	 */
	if (*RELOC(int *, &biosbasemem) == 0) {
		*RELOC(int *, &biosbasemem) = bl_biosbasemem;
		*RELOC(int *, &biosmem_implicit) = 1;
	}
	if (*RELOC(int *, &biosextmem) == 0) {
		*RELOC(int *, &biosextmem) = bl_biosextmem;
		*RELOC(int *, &biosmem_implicit) = 1;
	}
#undef RELOC
}

#endif /* XEN */

/*
 * Machine-dependent startup code
 */
void
cpu_startup(void)
{
	int x, y;
	vaddr_t minaddr, maxaddr;
	psize_t sz;

	/*
	 * For console drivers that require uvm and pmap to be initialized,
	 * we'll give them one more chance here...
	 */
	consinit();

	/*
	 * Initialize error message buffer (et end of core).
	 */
	if (msgbuf_p_cnt == 0)
		panic("msgbuf paddr map has not been set up");
	for (x = 0, sz = 0; x < msgbuf_p_cnt; sz += msgbuf_p_seg[x++].sz)
		continue;
	msgbuf_vaddr = uvm_km_alloc(kernel_map, sz, 0, UVM_KMF_VAONLY);
	if (msgbuf_vaddr == 0)
		panic("failed to valloc msgbuf_vaddr");

	/* msgbuf_paddr was init'd in pmap */
	for (y = 0, sz = 0; y < msgbuf_p_cnt; y++) {
		for (x = 0; x < btoc(msgbuf_p_seg[y].sz); x++, sz += PAGE_SIZE)
			pmap_kenter_pa((vaddr_t)msgbuf_vaddr + sz,
			    msgbuf_p_seg[y].paddr + x * PAGE_SIZE,
			    VM_PROT_READ|VM_PROT_WRITE, 0);
	}
	pmap_update(pmap_kernel());

	initmsgbuf((void *)msgbuf_vaddr, sz);

#ifdef MULTIBOOT
	multiboot_print_info();
#endif

#ifdef TRAPLOG
	/*
	 * Enable recording of branch from/to in MSR's
	 */
	wrmsr(MSR_DEBUGCTLMSR, 0x1);
#endif

#if NCARDBUS > 0
	/* Tell RBUS how much RAM we have, so it can use heuristics. */
	rbus_min_start_hint(ctob((psize_t)physmem));
#endif

	minaddr = 0;

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, false, NULL);

	/* Say hello. */
	banner();

	/* Safe for i/o port / memory space allocation to use malloc now. */
#if NISA > 0 || NPCI > 0
	x86_bus_space_mallocok();
#endif

	gdt_init();
	i386_proc0_tss_ldt_init();

#ifndef XEN
	cpu_init_tss(&cpu_info_primary);
	ltr(cpu_info_primary.ci_tss_sel);
#endif

	x86_startup();
}

/*
 * Set up proc0's TSS and LDT.
 */
void
i386_proc0_tss_ldt_init(void)
{
	struct lwp *l;
	struct pcb *pcb __diagused;

	l = &lwp0;
	pcb = lwp_getpcb(l);

	pmap_kernel()->pm_ldt_sel = GSEL(GLDT_SEL, SEL_KPL);
	pcb->pcb_cr0 = rcr0() & ~CR0_TS;
	pcb->pcb_esp0 = uvm_lwp_getuarea(l) + USPACE - 16;
	pcb->pcb_iopl = SEL_KPL;
	l->l_md.md_regs = (struct trapframe *)pcb->pcb_esp0 - 1;
	memcpy(&pcb->pcb_fsd, &gdt[GUDATA_SEL], sizeof(pcb->pcb_fsd));
	memcpy(&pcb->pcb_gsd, &gdt[GUDATA_SEL], sizeof(pcb->pcb_gsd));

#ifndef XEN
	lldt(pmap_kernel()->pm_ldt_sel);
#else
	HYPERVISOR_fpu_taskswitch(1);
	XENPRINTF(("lwp tss sp %p ss %04x/%04x\n",
	    (void *)pcb->pcb_esp0,
	    GSEL(GDATA_SEL, SEL_KPL),
	    IDXSEL(GSEL(GDATA_SEL, SEL_KPL))));
	HYPERVISOR_stack_switch(GSEL(GDATA_SEL, SEL_KPL), pcb->pcb_esp0);
#endif
}

#ifdef XEN
/* used in assembly */
void i386_switch_context(lwp_t *);
void i386_tls_switch(lwp_t *);

/*
 * Switch context:
 * - switch stack pointer for user->kernel transition
 */
void
i386_switch_context(lwp_t *l)
{
	struct pcb *pcb;
	struct physdev_op physop;

	pcb = lwp_getpcb(l);

	HYPERVISOR_stack_switch(GSEL(GDATA_SEL, SEL_KPL), pcb->pcb_esp0);

	physop.cmd = PHYSDEVOP_SET_IOPL;
	physop.u.set_iopl.iopl = pcb->pcb_iopl;
	HYPERVISOR_physdev_op(&physop);
}

void
i386_tls_switch(lwp_t *l)
{
	struct cpu_info *ci = curcpu();
	struct pcb *pcb = lwp_getpcb(l);
	/*
         * Raise the IPL to IPL_HIGH.
	 * FPU IPIs can alter the LWP's saved cr0.  Dropping the priority
	 * is deferred until mi_switch(), when cpu_switchto() returns.
	 */
	(void)splhigh();

        /*
	 * If our floating point registers are on a different CPU,
	 * set CR0_TS so we'll trap rather than reuse bogus state.
	 */

	if (l != ci->ci_fpcurlwp) {
		HYPERVISOR_fpu_taskswitch(1);
	}

	/* Update TLS segment pointers */
	update_descriptor(&ci->ci_gdt[GUFS_SEL],
			  (union descriptor *) &pcb->pcb_fsd);
	update_descriptor(&ci->ci_gdt[GUGS_SEL], 
			  (union descriptor *) &pcb->pcb_gsd);

}
#endif /* XEN */

#ifndef XEN
/*
 * Set up TSS and I/O bitmap.
 */
void
cpu_init_tss(struct cpu_info *ci)
{
	struct i386tss *tss = &ci->ci_tss;

	tss->tss_iobase = IOMAP_INVALOFF << 16;
	tss->tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	tss->tss_ldt = GSEL(GLDT_SEL, SEL_KPL);
	tss->tss_cr3 = rcr3();
	ci->ci_tss_sel = tss_alloc(tss);
}
#endif /* XEN */

void *
getframe(struct lwp *l, int sig, int *onstack)
{
	struct proc *p = l->l_proc;
	struct trapframe *tf = l->l_md.md_regs;

	/* Do we need to jump onto the signal stack? */
	*onstack = (l->l_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0
	    && (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;
	if (*onstack)
		return (char *)l->l_sigstk.ss_sp + l->l_sigstk.ss_size;
#ifdef VM86
	if (tf->tf_eflags & PSL_VM)
		return (void *)(tf->tf_esp + (tf->tf_ss << 4));
	else
#endif
		return (void *)tf->tf_esp;
}

/*
 * Build context to run handler in.  We invoke the handler
 * directly, only returning via the trampoline.  Note the
 * trampoline version numbers are coordinated with machine-
 * dependent code in libc.
 */
void
buildcontext(struct lwp *l, int sel, void *catcher, void *fp)
{
	struct trapframe *tf = l->l_md.md_regs;

	tf->tf_gs = GSEL(GUGS_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUFS_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_eip = (int)catcher;
	tf->tf_cs = GSEL(sel, SEL_UPL);
	tf->tf_eflags &= ~PSL_CLEARSIG;
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);

	/* Ensure FP state is reset. */
	fpu_save_area_reset(l);
}

void
sendsig_siginfo(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct pmap *pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	int sel = pmap->pm_hiexec > I386_MAX_EXE_ADDR ?
	    GUCODEBIG_SEL : GUCODE_SEL;
	struct sigacts *ps = p->p_sigacts;
	int onstack, error;
	int sig = ksi->ksi_signo;
	struct sigframe_siginfo *fp = getframe(l, sig, &onstack), frame;
	sig_t catcher = SIGACTION(p, sig).sa_handler;
	struct trapframe *tf = l->l_md.md_regs;

	KASSERT(mutex_owned(p->p_lock));

	fp--;

	frame.sf_ra = (int)ps->sa_sigdesc[sig].sd_tramp;
	frame.sf_signum = sig;
	frame.sf_sip = &fp->sf_si;
	frame.sf_ucp = &fp->sf_uc;
	frame.sf_si._info = ksi->ksi_info;
	frame.sf_uc.uc_flags = _UC_SIGMASK|_UC_VM;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_link = l->l_ctxlink;
	frame.sf_uc.uc_flags |= (l->l_sigstk.ss_flags & SS_ONSTACK)
	    ? _UC_SETSTACK : _UC_CLRSTACK;
	memset(&frame.sf_uc.uc_stack, 0, sizeof(frame.sf_uc.uc_stack));

	if (tf->tf_eflags & PSL_VM)
		(*p->p_emul->e_syscall_intern)(p);
	sendsig_reset(l, sig);

	mutex_exit(p->p_lock);
	cpu_getmcontext(l, &frame.sf_uc.uc_mcontext, &frame.sf_uc.uc_flags);
	error = copyout(&frame, fp, sizeof(frame));
	mutex_enter(p->p_lock);

	if (error != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	buildcontext(l, sel, catcher, fp);

	/* Remember that we're now on the signal stack. */
	if (onstack)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
}

static void
maybe_dump(int howto)
{
	int s;

	/* Disable interrupts. */
	s = splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();

	splx(s);
}

void
cpu_reboot(int howto, char *bootstr)
{
	static bool syncdone = false;
	int s = IPL_NONE;

	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;

	/* XXX used to dump after vfs_shutdown() and before
	 * detaching devices / shutdown hooks / pmf_system_shutdown().
	 */
	maybe_dump(howto);

	/*
	 * If we've panic'd, don't make the situation potentially
	 * worse by syncing or unmounting the file systems.
	 */
	if ((howto & RB_NOSYNC) == 0 && panicstr == NULL) {
		if (!syncdone) {
			syncdone = true;
			/* XXX used to force unmount as well, here */
			vfs_sync_all(curlwp);
			/*
			 * If we've been adjusting the clock, the todr
			 * will be out of synch; adjust it now.
			 *
			 * XXX used to do this after unmounting all
			 * filesystems with vfs_shutdown().
			 */
			if (time_adjusted != 0)
				resettodr();
		}

		while (vfs_unmountall1(curlwp, false, false) ||
		       config_detach_all(boothowto) ||
		       vfs_unmount_forceone(curlwp))
			;	/* do nothing */
	} else
		suspendsched();

	pmf_system_shutdown(boothowto);

	s = splhigh();

	/* amd64 maybe_dump() */

haltsys:
	doshutdownhooks();

	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NACPICA > 0
		if (s != IPL_NONE)
			splx(s);

		acpi_enter_sleep_state(ACPI_STATE_S5);
#else
		__USE(s);
#endif
#ifdef XEN
		HYPERVISOR_shutdown();
		for (;;);
#endif
	}

#ifdef MULTIPROCESSOR
	cpu_broadcast_halt();
#endif /* MULTIPROCESSOR */

	if (howto & RB_HALT) {
#if NACPICA > 0
		acpi_disable();
#endif

		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");

#ifdef BEEP_ONHALT
		{
			int c;
			for (c = BEEP_ONHALT_COUNT; c > 0; c--) {
				sysbeep(BEEP_ONHALT_PITCH,
					BEEP_ONHALT_PERIOD * hz / 1000);
				delay(BEEP_ONHALT_PERIOD * 1000);
				sysbeep(0, BEEP_ONHALT_PERIOD * hz / 1000);
				delay(BEEP_ONHALT_PERIOD * 1000);
			}
		}
#endif

		cnpollc(1);	/* for proper keyboard command handling */
		if (cngetc() == 0) {
			/* no console attached, so just hlt */
			printf("No keyboard - cannot reboot after all.\n");
			for(;;) {
				x86_hlt();
			}
		}
		cnpollc(0);
	}

	printf("rebooting...\n");
	if (cpureset_delay > 0)
		delay(cpureset_delay * 1000);
	cpu_reset();
	for(;;) ;
	/*NOTREACHED*/
}

/*
 * Clear registers on exec
 */
void
setregs(struct lwp *l, struct exec_package *pack, vaddr_t stack)
{
	struct pmap *pmap = vm_map_pmap(&l->l_proc->p_vmspace->vm_map);
	struct pcb *pcb = lwp_getpcb(l);
	struct trapframe *tf;

#ifdef USER_LDT
	pmap_ldt_cleanup(l);
#endif

	fpu_save_area_clear(l, pack->ep_osversion >= 699002600
	    ? __INITIAL_NPXCW__ : __NetBSD_COMPAT_NPXCW__);

	memcpy(&pcb->pcb_fsd, &gdt[GUDATA_SEL], sizeof(pcb->pcb_fsd));
	memcpy(&pcb->pcb_gsd, &gdt[GUDATA_SEL], sizeof(pcb->pcb_gsd));

	tf = l->l_md.md_regs;
	tf->tf_gs = GSEL(GUGS_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUFS_SEL, SEL_UPL);
	tf->tf_es = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_ds = LSEL(LUDATA_SEL, SEL_UPL);
	tf->tf_edi = 0;
	tf->tf_esi = 0;
	tf->tf_ebp = 0;
	tf->tf_ebx = l->l_proc->p_psstrp;
	tf->tf_edx = 0;
	tf->tf_ecx = 0;
	tf->tf_eax = 0;
	tf->tf_eip = pack->ep_entry;
	tf->tf_cs = pmap->pm_hiexec > I386_MAX_EXE_ADDR ?
	    LSEL(LUCODEBIG_SEL, SEL_UPL) : LSEL(LUCODE_SEL, SEL_UPL);
	tf->tf_eflags = PSL_USERSET;
	tf->tf_esp = stack;
	tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);
}

/*
 * Initialize segments and descriptor tables
 */

union	descriptor *gdt, *ldt;
union	descriptor *pentium_idt;
extern vaddr_t lwp0uarea;

void
setgate(struct gate_descriptor *gd, void *func, int args, int type, int dpl,
    int sel)
{

	gd->gd_looffset = (int)func;
	gd->gd_selector = sel;
	gd->gd_stkcpy = args;
	gd->gd_xx = 0;
	gd->gd_type = type;
	gd->gd_dpl = dpl;
	gd->gd_p = 1;
	gd->gd_hioffset = (int)func >> 16;
}

void
unsetgate(struct gate_descriptor *gd)
{
	gd->gd_p = 0;
	gd->gd_hioffset = 0;
	gd->gd_looffset = 0;
	gd->gd_selector = 0;
	gd->gd_xx = 0;
	gd->gd_stkcpy = 0;
	gd->gd_type = 0;
	gd->gd_dpl = 0;
}


void
setregion(struct region_descriptor *rd, void *base, size_t limit)
{

	rd->rd_limit = (int)limit;
	rd->rd_base = (int)base;
}

void
setsegment(struct segment_descriptor *sd, const void *base, size_t limit,
    int type, int dpl, int def32, int gran)
{

	sd->sd_lolimit = (int)limit;
	sd->sd_lobase = (int)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (int)limit >> 16;
	sd->sd_xx = 0;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (int)base >> 24;
}

#define	IDTVEC(name)	__CONCAT(X, name)
typedef void (vector)(void);
extern vector IDTVEC(syscall);
extern vector IDTVEC(osyscall);
extern vector *IDTVEC(exceptions)[];
extern vector IDTVEC(svr4_fasttrap);
void (*svr4_fasttrap_vec)(void) = (void (*)(void))nullop;
krwlock_t svr4_fasttrap_lock;
#ifdef XEN
#define MAX_XEN_IDT 128
trap_info_t xen_idt[MAX_XEN_IDT];
int xen_idt_idx;
extern union descriptor tmpgdt[];
#endif

void 
cpu_init_idt(void)
{
#ifndef XEN
	struct region_descriptor region;
	setregion(&region, pentium_idt, NIDT * sizeof(idt[0]) - 1);
	lidt(&region);
#else /* XEN */
	XENPRINTF(("HYPERVISOR_set_trap_table %p\n", xen_idt));
	if (HYPERVISOR_set_trap_table(xen_idt))
		panic("HYPERVISOR_set_trap_table %p failed\n", xen_idt);
#endif /* !XEN */
}

void
initgdt(union descriptor *tgdt)
{
	KASSERT(tgdt != NULL);
	
	gdt = tgdt;
#ifdef XEN
	u_long	frames[16];
#else
	struct region_descriptor region;
	memset(gdt, 0, NGDT*sizeof(*gdt));
#endif /* XEN */
	/* make gdt gates and memory segments */
	setsegment(&gdt[GCODE_SEL].sd, 0, 0xfffff, SDT_MEMERA, SEL_KPL, 1, 1);
	setsegment(&gdt[GDATA_SEL].sd, 0, 0xfffff, SDT_MEMRWA, SEL_KPL, 1, 1);
	setsegment(&gdt[GUCODE_SEL].sd, 0, x86_btop(I386_MAX_EXE_ADDR) - 1,
	    SDT_MEMERA, SEL_UPL, 1, 1);
	setsegment(&gdt[GUCODEBIG_SEL].sd, 0, 0xfffff,
	    SDT_MEMERA, SEL_UPL, 1, 1);
	setsegment(&gdt[GUDATA_SEL].sd, 0, 0xfffff,
	    SDT_MEMRWA, SEL_UPL, 1, 1);
#if NBIOSCALL > 0
	/* bios trampoline GDT entries */
	setsegment(&gdt[GBIOSCODE_SEL].sd, 0, 0xfffff, SDT_MEMERA, SEL_KPL, 0,
	    0);
	setsegment(&gdt[GBIOSDATA_SEL].sd, 0, 0xfffff, SDT_MEMRWA, SEL_KPL, 0,
	    0);
#endif
	setsegment(&gdt[GCPU_SEL].sd, &cpu_info_primary, 0xfffff,
	    SDT_MEMRWA, SEL_KPL, 1, 1);

#ifndef XEN
	setregion(&region, gdt, NGDT * sizeof(gdt[0]) - 1);
	lgdt(&region);
#else /* !XEN */
	/*
	 * We jumpstart the bootstrap process a bit so we can update
	 * page permissions. This is done redundantly later from
	 * x86_xpmap.c:xen_pmap_bootstrap() - harmless.
	 */
	xpmap_phys_to_machine_mapping =
	    (unsigned long *)xen_start_info.mfn_list;

	frames[0] = xpmap_ptom((uint32_t)gdt - KERNBASE) >> PAGE_SHIFT;
	{	/*
		 * Enter the gdt page RO into the kernel map. We can't
		 * use pmap_kenter_pa() here, because %fs is not
		 * usable until the gdt is loaded, and %fs is used as
		 * the base pointer for curcpu() and curlwp(), both of
		 * which are in the callpath of pmap_kenter_pa().
		 * So we mash up our own - this is MD code anyway.
		 */
		pt_entry_t pte;
		pt_entry_t pg_nx = (cpu_feature[2] & CPUID_NOX ? PG_NX : 0);

		pte = pmap_pa2pte((vaddr_t)gdt - KERNBASE);
		pte |= PG_k | PG_RO | pg_nx | PG_V;

		if (HYPERVISOR_update_va_mapping((vaddr_t)gdt, pte, UVMF_INVLPG) < 0) {
			panic("gdt page RO update failed.\n");
		}

	}

	XENPRINTK(("loading gdt %lx, %d entries\n", frames[0] << PAGE_SHIFT,
	    NGDT));
	if (HYPERVISOR_set_gdt(frames, NGDT /* XXX is it right ? */))
		panic("HYPERVISOR_set_gdt failed!\n");

	lgdt_finish();
#endif /* !XEN */
}

static void
init386_msgbuf(void)
{
	/* Message buffer is located at end of core. */
	struct vm_physseg *vps;
	psize_t sz = round_page(MSGBUFSIZE);
	psize_t reqsz = sz;
	unsigned int x;

 search_again:
	vps = NULL;
	for (x = 0; x < vm_nphysseg; ++x) {
		vps = VM_PHYSMEM_PTR(x);
		if (ctob(vps->avail_end) == avail_end) {
			break;
		}
	}
	if (x == vm_nphysseg)
		panic("init386: can't find end of memory");

	/* Shrink so it'll fit in the last segment. */
	if (vps->avail_end - vps->avail_start < atop(sz))
		sz = ctob(vps->avail_end - vps->avail_start);

	vps->avail_end -= atop(sz);
	vps->end -= atop(sz);
	msgbuf_p_seg[msgbuf_p_cnt].sz = sz;
	msgbuf_p_seg[msgbuf_p_cnt++].paddr = ctob(vps->avail_end);

	/* Remove the last segment if it now has no pages. */
	if (vps->start == vps->end) {
		for (--vm_nphysseg; x < vm_nphysseg; x++)
			VM_PHYSMEM_PTR_SWAP(x, x + 1);
	}

	/* Now find where the new avail_end is. */
	for (avail_end = 0, x = 0; x < vm_nphysseg; x++)
		if (VM_PHYSMEM_PTR(x)->avail_end > avail_end)
			avail_end = VM_PHYSMEM_PTR(x)->avail_end;
	avail_end = ctob(avail_end);

	if (sz == reqsz)
		return;

	reqsz -= sz;
	if (msgbuf_p_cnt == VM_PHYSSEG_MAX) {
		/* No more segments available, bail out. */
		printf("WARNING: MSGBUFSIZE (%zu) too large, using %zu.\n",
		    (size_t)MSGBUFSIZE, (size_t)(MSGBUFSIZE - reqsz));
		return;
	}

	sz = reqsz;
	goto search_again;
}

#ifndef XEN
static void
init386_pte0(void)
{
	paddr_t paddr;
	vaddr_t vaddr;

	paddr = 4 * PAGE_SIZE;
	vaddr = (vaddr_t)vtopte(0);
	pmap_kenter_pa(vaddr, paddr, VM_PROT_ALL, 0);
	pmap_update(pmap_kernel());
	/* make sure it is clean before using */
	memset((void *)vaddr, 0, PAGE_SIZE);
}
#endif /* !XEN */

static void
init386_ksyms(void)
{
#if NKSYMS || defined(DDB) || defined(MODULAR)
	extern int end;
	struct btinfo_symtab *symtab;

#ifdef DDB
	db_machine_init();
#endif

#if defined(MULTIBOOT)
	if (multiboot_ksyms_addsyms_elf())
		return;
#endif

	if ((symtab = lookup_bootinfo(BTINFO_SYMTAB)) == NULL) {
		ksyms_addsyms_elf(*(int *)&end, ((int *)&end) + 1, esym);
		return;
	}

	symtab->ssym += KERNBASE;
	symtab->esym += KERNBASE;
	ksyms_addsyms_elf(symtab->nsym, (int *)symtab->ssym, (int *)symtab->esym);
#endif
}

void
init386(paddr_t first_avail)
{
	extern void consinit(void);
	int x;
#ifndef XEN
	union descriptor *tgdt;
	extern struct extent *iomem_ex;
	struct region_descriptor region;
	struct btinfo_memmap *bim;
#endif
#if NBIOSCALL > 0
	extern int biostramp_image_size;
	extern u_char biostramp_image[];
#endif

#ifdef XEN
	XENPRINTK(("HYPERVISOR_shared_info %p (%x)\n", HYPERVISOR_shared_info,
	    xen_start_info.shared_info));
	KASSERT(HYPERVISOR_shared_info != NULL);
	cpu_info_primary.ci_vcpu = &HYPERVISOR_shared_info->vcpu_info[0];
#endif
	cpu_probe(&cpu_info_primary);

	uvm_lwp_setuarea(&lwp0, lwp0uarea);

	cpu_init_msrs(&cpu_info_primary, true);

#ifdef PAE
	use_pae = 1;
#else
	use_pae = 0;
#endif

#ifdef XEN
	struct pcb *pcb = lwp_getpcb(&lwp0);
	pcb->pcb_cr3 = PDPpaddr;
	__PRINTK(("pcb_cr3 0x%lx cr3 0x%lx\n",
	    PDPpaddr, xpmap_ptom(PDPpaddr)));
	XENPRINTK(("lwp0uarea %p first_avail %p\n",
	    lwp0uarea, (void *)(long)first_avail));
	XENPRINTK(("ptdpaddr %p atdevbase %p\n", (void *)PDPpaddr,
	    (void *)atdevbase));
#endif

#if defined(PAE) && !defined(XEN)
	/*
	 * Save VA and PA of L3 PD of boot processor (for Xen, this is done
	 * in xen_pmap_bootstrap())
	 */
	cpu_info_primary.ci_pae_l3_pdirpa = rcr3();
	cpu_info_primary.ci_pae_l3_pdir = (pd_entry_t *)(rcr3() + KERNBASE);
#endif /* PAE && !XEN */

#ifdef XEN
	xen_parse_cmdline(XEN_PARSE_BOOTFLAGS, NULL);
#endif

	/*
	 * Initialize PAGE_SIZE-dependent variables.
	 */
	uvm_setpagesize();

	/*
	 * Start with 2 color bins -- this is just a guess to get us
	 * started.  We'll recolor when we determine the largest cache
	 * sizes on the system.
	 */
	uvmexp.ncolors = 2;

#ifndef XEN
	/*
	 * Low memory reservations:
	 * Page 0:	BIOS data
	 * Page 1:	BIOS callback
	 * Page 2:	MP bootstrap
	 * Page 3:	ACPI wakeup code
	 * Page 4:	Temporary page table for 0MB-4MB
	 * Page 5:	Temporary page directory
	 */
	avail_start = 6 * PAGE_SIZE;
#else /* !XEN */
	/* steal one page for gdt */
	gdt = (void *)((u_long)first_avail + KERNBASE);
	first_avail += PAGE_SIZE;
	/* Make sure the end of the space used by the kernel is rounded. */
	first_avail = round_page(first_avail);
	avail_start = first_avail;
	avail_end = ctob((paddr_t)xen_start_info.nr_pages);
	pmap_pa_start = (KERNTEXTOFF - KERNBASE);
	pmap_pa_end = pmap_pa_start + ctob((paddr_t)xen_start_info.nr_pages);
	mem_clusters[0].start = avail_start;
	mem_clusters[0].size = avail_end - avail_start;
	mem_cluster_cnt++;
	physmem += xen_start_info.nr_pages;
	uvmexp.wired += atop(avail_start);
	/*
	 * initgdt() has to be done before consinit(), so that %fs is properly
	 * initialised. initgdt() uses pmap_kenter_pa so it can't be called
	 * before the above variables are set.
	 */

	initgdt(gdt);

	mutex_init(&pte_lock, MUTEX_DEFAULT, IPL_VM);
#endif /* XEN */

#if NISA > 0 || NPCI > 0
	x86_bus_space_init();
#endif /* NISA > 0 || NPCI > 0 */
	
	consinit();	/* XXX SHOULD NOT BE DONE HERE */

#ifdef DEBUG_MEMLOAD
	printf("mem_cluster_count: %d\n", mem_cluster_cnt);
#endif

	/*
	 * Call pmap initialization to make new kernel address space.
	 * We must do this before loading pages into the VM system.
	 */
	pmap_bootstrap((vaddr_t)atdevbase + IOM_SIZE);

#ifndef XEN
	/*
	 * Check to see if we have a memory map from the BIOS (passed
	 * to us by the boot program.
	 */
	bim = lookup_bootinfo(BTINFO_MEMMAP);
	if ((biosmem_implicit || (biosbasemem == 0 && biosextmem == 0)) &&
	    bim != NULL && bim->num > 0)
		initx86_parse_memmap(bim, iomem_ex);

	/*
	 * If the loop above didn't find any valid segment, fall back to
	 * former code.
	 */
	if (mem_cluster_cnt == 0)
		initx86_fake_memmap(iomem_ex);

	initx86_load_memmap(first_avail);

#else /* !XEN */
	XENPRINTK(("load the memory cluster 0x%" PRIx64 " (%" PRId64 ") - "
	    "0x%" PRIx64 " (%" PRId64 ")\n",
	    (uint64_t)avail_start, (uint64_t)atop(avail_start),
	    (uint64_t)avail_end, (uint64_t)atop(avail_end)));
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end),
	    VM_FREELIST_DEFAULT);

	/* Reclaim the boot gdt page - see locore.s */
	{
		pt_entry_t pte;
		pt_entry_t pg_nx = (cpu_feature[2] & CPUID_NOX ? PG_NX : 0);

		pte = pmap_pa2pte((vaddr_t)tmpgdt - KERNBASE);
		pte |= PG_k | PG_RW | pg_nx | PG_V;

		if (HYPERVISOR_update_va_mapping((vaddr_t)tmpgdt, pte, UVMF_INVLPG) < 0) {
			panic("tmpgdt page relaim RW update failed.\n");
		}
	}

#endif /* !XEN */

	init386_msgbuf();

#ifndef XEN
	/*
	 * XXX Remove this
	 *
	 * Setup a temporary Page Table Entry to allow identity mappings of
	 * the real mode address. This is required by:
	 * - bioscall
	 * - MP bootstrap
	 * - ACPI wakecode
	 */
	init386_pte0();

#if NBIOSCALL > 0
	KASSERT(biostramp_image_size <= PAGE_SIZE);
	pmap_kenter_pa((vaddr_t)BIOSTRAMP_BASE,	/* virtual */
		       (paddr_t)BIOSTRAMP_BASE,	/* physical */
		       VM_PROT_ALL, 0);		/* protection */
	pmap_update(pmap_kernel());
	memcpy((void *)BIOSTRAMP_BASE, biostramp_image, biostramp_image_size);

	/* Needed early, for bioscall() */
	cpu_info_primary.ci_pmap = pmap_kernel();
#endif
#endif /* !XEN */

	pmap_kenter_pa(idt_vaddr, idt_paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_update(pmap_kernel());
	memset((void *)idt_vaddr, 0, PAGE_SIZE);


#ifndef XEN
	idt_init();

	idt = (struct gate_descriptor *)idt_vaddr;
	pmap_kenter_pa(pentium_idt_vaddr, idt_paddr, VM_PROT_READ, 0);
	pmap_update(pmap_kernel());
	pentium_idt = (union descriptor *)pentium_idt_vaddr;

	tgdt = gdt;
	gdt = (union descriptor *)
		    ((char *)idt + NIDT * sizeof (struct gate_descriptor));
	ldt = gdt + NGDT;

	memcpy(gdt, tgdt, NGDT*sizeof(*gdt));

	setsegment(&gdt[GLDT_SEL].sd, ldt, NLDT * sizeof(ldt[0]) - 1,
	    SDT_SYSLDT, SEL_KPL, 0, 0);
#else
	HYPERVISOR_set_callbacks(
	    GSEL(GCODE_SEL, SEL_KPL), (unsigned long)hypervisor_callback,
	    GSEL(GCODE_SEL, SEL_KPL), (unsigned long)failsafe_callback);

	ldt = (union descriptor *)idt_vaddr;
#endif /* XEN */

	/* make ldt gates and memory segments */
	setgate(&ldt[LSYS5CALLS_SEL].gd, &IDTVEC(osyscall), 1,
	    SDT_SYS386CGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));

	ldt[LUCODE_SEL] = gdt[GUCODE_SEL];
	ldt[LUCODEBIG_SEL] = gdt[GUCODEBIG_SEL];
	ldt[LUDATA_SEL] = gdt[GUDATA_SEL];
	ldt[LSOL26CALLS_SEL] = ldt[LBSDICALLS_SEL] = ldt[LSYS5CALLS_SEL];

#ifndef XEN
	/* exceptions */
	for (x = 0; x < 32; x++) {
		idt_vec_reserve(x);
		setgate(&idt[x], IDTVEC(exceptions)[x], 0, SDT_SYS386IGT,
		    (x == 3 || x == 4) ? SEL_UPL : SEL_KPL,
		    GSEL(GCODE_SEL, SEL_KPL));
	}

	/* new-style interrupt gate for syscalls */
	idt_vec_reserve(128);
	setgate(&idt[128], &IDTVEC(syscall), 0, SDT_SYS386IGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	idt_vec_reserve(0xd2);
	setgate(&idt[0xd2], &IDTVEC(svr4_fasttrap), 0, SDT_SYS386IGT,
	    SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));

	setregion(&region, gdt, NGDT * sizeof(gdt[0]) - 1);
	lgdt(&region);

	cpu_init_idt();
#else /* !XEN */
	memset(xen_idt, 0, sizeof(trap_info_t) * MAX_XEN_IDT);
	xen_idt_idx = 0;
	for (x = 0; x < 32; x++) {
		KASSERT(xen_idt_idx < MAX_XEN_IDT);
		xen_idt[xen_idt_idx].vector = x;

		switch (x) {
		case 2:  /* NMI */
		case 18: /* MCA */
			TI_SET_IF(&(xen_idt[xen_idt_idx]), 2);
			break;
		case 3:
		case 4:
			xen_idt[xen_idt_idx].flags = SEL_UPL;
			break;
		default:
			xen_idt[xen_idt_idx].flags = SEL_XEN;
			break;
		}

		xen_idt[xen_idt_idx].cs = GSEL(GCODE_SEL, SEL_KPL);
		xen_idt[xen_idt_idx].address =
			(uint32_t)IDTVEC(exceptions)[x];
		xen_idt_idx++;
	}
	KASSERT(xen_idt_idx < MAX_XEN_IDT);
	xen_idt[xen_idt_idx].vector = 128;
	xen_idt[xen_idt_idx].flags = SEL_UPL;
	xen_idt[xen_idt_idx].cs = GSEL(GCODE_SEL, SEL_KPL);
	xen_idt[xen_idt_idx].address = (uint32_t)&IDTVEC(syscall);
	xen_idt_idx++;
	KASSERT(xen_idt_idx < MAX_XEN_IDT);
	xen_idt[xen_idt_idx].vector = 0xd2;
	xen_idt[xen_idt_idx].flags = SEL_UPL;
	xen_idt[xen_idt_idx].cs = GSEL(GCODE_SEL, SEL_KPL);
	xen_idt[xen_idt_idx].address = (uint32_t)&IDTVEC(svr4_fasttrap);
	xen_idt_idx++;
	lldt(GSEL(GLDT_SEL, SEL_KPL));
	cpu_init_idt();
#endif /* XEN */

	init386_ksyms();

#if NMCA > 0
	/* check for MCA bus, needed to be done before ISA stuff - if
	 * MCA is detected, ISA needs to use level triggered interrupts
	 * by default */
	mca_busprobe();
#endif

#ifdef XEN
	XENPRINTF(("events_default_setup\n"));
	events_default_setup();
#else
	intr_default_setup();
#endif

	splraise(IPL_HIGH);
	x86_enable_intr();

#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef IPKDB
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#endif
#ifdef KGDB
	kgdb_port_init();
	if (boothowto & RB_KDB) {
		kgdb_debug_init = 1;
		kgdb_connect(1);
	}
#endif

	if (physmem < btoc(2 * 1024 * 1024)) {
		printf("warning: too little memory available; "
		       "have %lu bytes, want %lu bytes\n"
		       "running in degraded mode\n"
		       "press a key to confirm\n\n",
		       (unsigned long)ptoa(physmem), 2*1024*1024UL);
		cngetc();
	}

	rw_init(&svr4_fasttrap_lock);
}

#include <dev/ic/mc146818reg.h>		/* for NVRAM POST */
#include <i386/isa/nvram.h>		/* for NVRAM POST */

void
cpu_reset(void)
{
#ifdef XEN
	HYPERVISOR_reboot();
	for (;;);
#else /* XEN */
	struct region_descriptor region;

	x86_disable_intr();

	/*
	 * Ensure the NVRAM reset byte contains something vaguely sane.
	 */

	outb(IO_RTC, NVRAM_RESET);
	outb(IO_RTC+1, NVRAM_RESET_RST);

	/*
	 * Reset AMD Geode SC1100.
	 *
	 * 1) Write PCI Configuration Address Register (0xcf8) to
	 *    select Function 0, Register 0x44: Bridge Configuration,
	 *    GPIO and LPC Configuration Register Space, Reset
	 *    Control Register.
	 *
	 * 2) Write 0xf to PCI Configuration Data Register (0xcfc)
	 *    to reset IDE controller, IDE bus, and PCI bus, and
	 *    to trigger a system-wide reset.
	 * 
	 * See AMD Geode SC1100 Processor Data Book, Revision 2.0,
	 * sections 6.3.1, 6.3.2, and 6.4.1.
	 */
	if (cpu_info_primary.ci_signature == 0x540) {
		outl(0xcf8, 0x80009044);
		outl(0xcfc, 0xf);
	}

	x86_reset();

	/*
	 * Try to cause a triple fault and watchdog reset by making the IDT
	 * invalid and causing a fault.
	 */
	memset((void *)idt, 0, NIDT * sizeof(idt[0]));
	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
	lidt(&region);
	breakpoint();

#if 0
	/*
	 * Try to cause a triple fault and watchdog reset by unmapping the
	 * entire address space and doing a TLB flush.
	 */
	memset((void *)PTD, 0, PAGE_SIZE);
	tlbflush();
#endif

	for (;;);
#endif /* XEN */
}

void
cpu_getmcontext(struct lwp *l, mcontext_t *mcp, unsigned int *flags)
{
	const struct trapframe *tf = l->l_md.md_regs;
	__greg_t *gr = mcp->__gregs;
	__greg_t ras_eip;

	/* Save register context. */
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		gr[_REG_GS]  = tf->tf_vm86_gs;
		gr[_REG_FS]  = tf->tf_vm86_fs;
		gr[_REG_ES]  = tf->tf_vm86_es;
		gr[_REG_DS]  = tf->tf_vm86_ds;
		gr[_REG_EFL] = get_vflags(l);
	} else
#endif
	{
		gr[_REG_GS]  = tf->tf_gs;
		gr[_REG_FS]  = tf->tf_fs;
		gr[_REG_ES]  = tf->tf_es;
		gr[_REG_DS]  = tf->tf_ds;
		gr[_REG_EFL] = tf->tf_eflags;
	}
	gr[_REG_EDI]    = tf->tf_edi;
	gr[_REG_ESI]    = tf->tf_esi;
	gr[_REG_EBP]    = tf->tf_ebp;
	gr[_REG_EBX]    = tf->tf_ebx;
	gr[_REG_EDX]    = tf->tf_edx;
	gr[_REG_ECX]    = tf->tf_ecx;
	gr[_REG_EAX]    = tf->tf_eax;
	gr[_REG_EIP]    = tf->tf_eip;
	gr[_REG_CS]     = tf->tf_cs;
	gr[_REG_ESP]    = tf->tf_esp;
	gr[_REG_UESP]   = tf->tf_esp;
	gr[_REG_SS]     = tf->tf_ss;
	gr[_REG_TRAPNO] = tf->tf_trapno;
	gr[_REG_ERR]    = tf->tf_err;

	if ((ras_eip = (__greg_t)ras_lookup(l->l_proc,
	    (void *) gr[_REG_EIP])) != -1)
		gr[_REG_EIP] = ras_eip;

	*flags |= _UC_CPU;

	mcp->_mc_tlsbase = (uintptr_t)l->l_private;
	*flags |= _UC_TLSBASE;

	/*
	 * Save floating point register context.
	 *
	 * If the cpu doesn't support fxsave we must still write to
	 * the entire 512 byte area - otherwise we leak kernel memory
	 * contents to userspace.
	 * It wouldn't matter if we were doing the copyout here.
	 * So we might as well convert to fxsave format.
	 */
	__CTASSERT(sizeof (struct fxsave) ==
	    sizeof mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
	process_read_fpregs_xmm(l, (struct fxsave *)
	    &mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
	memset(&mcp->__fpregs.__fp_pad, 0, sizeof mcp->__fpregs.__fp_pad);
	*flags |= _UC_FXSAVE | _UC_FPU;
}

int
cpu_mcontext_validate(struct lwp *l, const mcontext_t *mcp)
{
	const __greg_t *gr = mcp->__gregs;
	struct trapframe *tf = l->l_md.md_regs;

	/*
	 * Check for security violations.  If we're returning
	 * to protected mode, the CPU will validate the segment
	 * registers automatically and generate a trap on
	 * violations.  We handle the trap, rather than doing
	 * all of the checking here.
	 */
	if (((gr[_REG_EFL] ^ tf->tf_eflags) & PSL_USERSTATIC) ||
	    !USERMODE(gr[_REG_CS], gr[_REG_EFL]))
		return EINVAL;

	return 0;
}

int
cpu_setmcontext(struct lwp *l, const mcontext_t *mcp, unsigned int flags)
{
	struct trapframe *tf = l->l_md.md_regs;
	const __greg_t *gr = mcp->__gregs;
	struct proc *p = l->l_proc;
	int error;

	/* Restore register context, if any. */
	if ((flags & _UC_CPU) != 0) {
#ifdef VM86
		if (gr[_REG_EFL] & PSL_VM) {
			tf->tf_vm86_gs = gr[_REG_GS];
			tf->tf_vm86_fs = gr[_REG_FS];
			tf->tf_vm86_es = gr[_REG_ES];
			tf->tf_vm86_ds = gr[_REG_DS];
			set_vflags(l, gr[_REG_EFL]);
			if (flags & _UC_VM) {
				void syscall_vm86(struct trapframe *);
				l->l_proc->p_md.md_syscall = syscall_vm86;
			}
		} else
#endif
		{
			error = cpu_mcontext_validate(l, mcp);
			if (error)
				return error;

			tf->tf_gs = gr[_REG_GS];
			tf->tf_fs = gr[_REG_FS];
			tf->tf_es = gr[_REG_ES];
			tf->tf_ds = gr[_REG_DS];
			/* Only change the user-alterable part of eflags */
			tf->tf_eflags &= ~PSL_USER;
			tf->tf_eflags |= (gr[_REG_EFL] & PSL_USER);
		}
		tf->tf_edi    = gr[_REG_EDI];
		tf->tf_esi    = gr[_REG_ESI];
		tf->tf_ebp    = gr[_REG_EBP];
		tf->tf_ebx    = gr[_REG_EBX];
		tf->tf_edx    = gr[_REG_EDX];
		tf->tf_ecx    = gr[_REG_ECX];
		tf->tf_eax    = gr[_REG_EAX];
		tf->tf_eip    = gr[_REG_EIP];
		tf->tf_cs     = gr[_REG_CS];
		tf->tf_esp    = gr[_REG_UESP];
		tf->tf_ss     = gr[_REG_SS];
	}

	if ((flags & _UC_TLSBASE) != 0)
		lwp_setprivate(l, (void *)(uintptr_t)mcp->_mc_tlsbase);

	/* Restore floating point register context, if given. */
	if ((flags & _UC_FPU) != 0) {
		__CTASSERT(sizeof (struct fxsave) ==
		    sizeof mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
		__CTASSERT(sizeof (struct save87) ==
		    sizeof mcp->__fpregs.__fp_reg_set.__fpchip_state);

		if (flags & _UC_FXSAVE) {
			process_write_fpregs_xmm(l, (const struct fxsave *)
				    &mcp->__fpregs.__fp_reg_set.__fp_xmm_state);
		} else {
			process_write_fpregs_s87(l, (const struct save87 *)
				    &mcp->__fpregs.__fp_reg_set.__fpchip_state);
		}
	}

	mutex_enter(p->p_lock);
	if (flags & _UC_SETSTACK)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
	if (flags & _UC_CLRSTACK)
		l->l_sigstk.ss_flags &= ~SS_ONSTACK;
	mutex_exit(p->p_lock);
	return (0);
}

void
cpu_initclocks(void)
{

	(*initclock_func)();
}

#define	DEV_IO 14		/* iopl for compat_10 */

int
mm_md_open(dev_t dev, int flag, int mode, struct lwp *l)
{

	switch (minor(dev)) {
	case DEV_IO:
		/*
		 * This is done by i386_iopl(3) now.
		 *
		 * #if defined(COMPAT_10) || defined(COMPAT_FREEBSD)
		 */
		if (flag & FWRITE) {
			struct trapframe *fp;
			int error;

			error = kauth_authorize_machdep(l->l_cred,
			    KAUTH_MACHDEP_IOPL, NULL, NULL, NULL, NULL);
			if (error)
				return (error);
			fp = curlwp->l_md.md_regs;
			fp->tf_eflags |= PSL_IOPL;
		}
		break;
	default:
		break;
	}
	return 0;
}

#ifdef PAE
void
cpu_alloc_l3_page(struct cpu_info *ci)
{
	int ret;
	struct pglist pg;
	struct vm_page *vmap;

	KASSERT(ci != NULL);
	/*
	 * Allocate a page for the per-CPU L3 PD. cr3 being 32 bits, PA musts
	 * resides below the 4GB boundary.
	 */
	ret = uvm_pglistalloc(PAGE_SIZE, 0, 0x100000000ULL, 32, 0, &pg, 1, 0);
	vmap = TAILQ_FIRST(&pg);

	if (ret != 0 || vmap == NULL)
		panic("%s: failed to allocate L3 pglist for CPU %d (ret %d)\n",
			__func__, cpu_index(ci), ret);

	ci->ci_pae_l3_pdirpa = vmap->phys_addr;

	ci->ci_pae_l3_pdir = (paddr_t *)uvm_km_alloc(kernel_map, PAGE_SIZE, 0,
		UVM_KMF_VAONLY | UVM_KMF_NOWAIT);
	if (ci->ci_pae_l3_pdir == NULL)
		panic("%s: failed to allocate L3 PD for CPU %d\n",
			__func__, cpu_index(ci));

	pmap_kenter_pa((vaddr_t)ci->ci_pae_l3_pdir, ci->ci_pae_l3_pdirpa,
		VM_PROT_READ | VM_PROT_WRITE, 0);

	pmap_update(pmap_kernel());
}
#endif /* PAE */
