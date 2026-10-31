/*	$NetBSD: machdep.c,v 1.366.2.1 2025/03/29 10:32:43 martin Exp $	*/

/*
 * Copyright (c) 1996, 1997, 1998, 2000, 2006, 2007, 2008, 2011
 *     The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc. which was written under contract to Coyote
 * Point by Jed Davis and Devon O'Dell.
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

/*
 * Copyright (c) 2006 Mathieu Ropert <mro@adviseo.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 2007 Manuel Bouyer.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
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
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.366.2.1 2025/03/29 10:32:43 martin Exp $");

#include "opt_modular.h"
#include "opt_user_ldt.h"
#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_cpureset_delay.h"
#include "opt_mtrr.h"
#include "opt_realmem.h"
#include "opt_xen.h"
#include "opt_svs.h"
#include "opt_kaslr.h"
#ifndef XENPV
#include "opt_physmem.h"
#endif
#include "isa.h"
#include "pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/exec.h>
#include <sys/exec_aout.h>	/* for MID_* */
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/ucontext.h>
#include <machine/kcore.h>
#include <sys/ras.h>
#include <sys/syscallargs.h>
#include <sys/ksyms.h>
#include <sys/device.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/asan.h>
#include <sys/csan.h>
#include <sys/msan.h>
#include <sys/module.h>
#include <sys/timevar.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <lib/libkern/entpool.h> /* XXX */

#include <dev/cons.h>
#include <dev/mm.h>

#include <uvm/uvm.h>
#include <uvm/uvm_page.h>

#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpu_rng.h>
#include <machine/cpufunc.h>
#include <machine/gdt.h>
#include <machine/intr.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/bootinfo.h>
#include <x86/fpu.h>
#include <x86/dbregs.h>
#include <machine/mtrr.h>
#include <machine/mpbiosvar.h>
#include <machine/pmap_private.h>

#include <x86/bootspace.h>
#include <x86/cputypes.h>
#include <x86/cpuvar.h>
#include <x86/machdep.h>
#include <x86/x86/tsc.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>
#include <dev/ic/i8042reg.h>

#ifdef XEN
#include <xen/xen.h>
#include <xen/hypervisor.h>
#include <xen/evtchn.h>
#include <xen/include/public/version.h>
#include <xen/include/public/vcpu.h>
#endif /* XEN */

#include <ddb/db_active.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_output.h>
#include <ddb/db_interface.h>
#endif

#include "acpica.h"

#if NACPICA > 0
#include <dev/acpi/acpivar.h>
#define ACPI_MACHDEP_PRIVATE
#include <machine/acpi_machdep.h>
#else
#include <machine/i82489var.h>
#endif

#include "isa.h"
#include "isadma.h"
#include "ksyms.h"

/* the following is used externally (sysctl_hw) */
char machine[] = "amd64";		/* CPU "architecture" */
char machine_arch[] = "x86_64";		/* machine == machine_arch */

#ifdef CPURESET_DELAY
int cpureset_delay = CPURESET_DELAY;
#else
int cpureset_delay = 2000; /* default to 2s */
#endif

int cpu_class = CPUCLASS_686;

#ifdef MTRR
const struct mtrr_funcs *mtrr_funcs;
#endif

int cpu_class;
int use_pae;

#ifndef NO_SPARSE_DUMP
int sparse_dump = 1;

paddr_t max_paddr = 0;
unsigned char *sparse_dump_physmap;
#endif

char *dump_headerbuf, *dump_headerbuf_ptr;
#define dump_headerbuf_size PAGE_SIZE
#define dump_headerbuf_end (dump_headerbuf + dump_headerbuf_size)
#define dump_headerbuf_avail (dump_headerbuf_end - dump_headerbuf_ptr)
daddr_t dump_header_blkno;

size_t dump_nmemsegs;
size_t dump_npages;
size_t dump_header_size;
size_t dump_totalbytesleft;

vaddr_t idt_vaddr;
paddr_t idt_paddr;
vaddr_t gdt_vaddr;
paddr_t gdt_paddr;
vaddr_t ldt_vaddr;
paddr_t ldt_paddr;

static struct vm_map module_map_store;
extern struct bootspace bootspace;
extern struct slotspace slotspace;

vaddr_t vm_min_kernel_address __read_mostly = VM_MIN_KERNEL_ADDRESS_DEFAULT;
vaddr_t vm_max_kernel_address __read_mostly = VM_MAX_KERNEL_ADDRESS_DEFAULT;
pd_entry_t *pte_base __read_mostly;

struct vm_map *phys_map = NULL;

extern paddr_t lowmem_rsvd;
extern paddr_t avail_start, avail_end;
#ifdef XENPV
extern paddr_t pmap_pa_start, pmap_pa_end;
#endif

struct nmistore {
	uint64_t cr3;
	uint64_t scratch;
} __packed;

/*
 * Size of memory segments, before any memory is stolen.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int mem_cluster_cnt;

int cpu_dump(void);
int cpu_dumpsize(void);
u_long cpu_dump_mempagecnt(void);
void dodumpsys(void);
void dumpsys(void);

static void x86_64_proc0_pcb_ldt_init(void);

void dump_misc_init(void);
void dump_seg_prep(void);
int dump_seg_iter(int (*)(paddr_t, paddr_t));

#ifndef NO_SPARSE_DUMP
void sparse_dump_reset(void);
void sparse_dump_mark(void);
void cpu_dump_prep_sparse(void);
#endif

void dump_header_start(void);
int dump_header_flush(void);
int dump_header_addbytes(const void*, size_t);
int dump_header_addseg(paddr_t, paddr_t);
int dump_header_finish(void);

int dump_seg_count_range(paddr_t, paddr_t);
int dumpsys_seg(paddr_t, paddr_t);

void init_bootspace(void);
void init_slotspace(void);
void init_x86_64(paddr_t);

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
	 * Initialize error message buffer (at end of core).
	 */
	if (msgbuf_p_cnt == 0)
		panic("msgbuf paddr map has not been set up");
	for (x = 0, sz = 0; x < msgbuf_p_cnt; sz += msgbuf_p_seg[x++].sz)
		continue;

	msgbuf_vaddr = uvm_km_alloc(kernel_map, sz, 0, UVM_KMF_VAONLY);
	if (msgbuf_vaddr == 0)
		panic("failed to valloc msgbuf_vaddr");

	for (y = 0, sz = 0; y < msgbuf_p_cnt; y++) {
		for (x = 0; x < btoc(msgbuf_p_seg[y].sz); x++, sz += PAGE_SIZE)
			pmap_kenter_pa((vaddr_t)msgbuf_vaddr + sz,
			    msgbuf_p_seg[y].paddr + x * PAGE_SIZE,
			    VM_PROT_READ|VM_PROT_WRITE, 0);
	}

	pmap_update(pmap_kernel());

	initmsgbuf((void *)msgbuf_vaddr, round_page(sz));

	minaddr = 0;

	/*
	 * Allocate a submap for physio.
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, false, NULL);

	/*
	 * Create the module map.
	 *
	 * The kernel uses RIP-relative addressing with a maximum offset of
	 * 2GB. Because of that, we can't put the kernel modules in kernel_map
	 * (like i386 does), since kernel_map is too far away in memory from
	 * the kernel sections. So we have to create a special module_map.
	 *
	 * The module map is taken as what is left of the bootstrap memory
	 * created in locore/prekern.
	 */
	uvm_map_setup(&module_map_store, bootspace.smodule,
	    bootspace.emodule, 0);
	module_map_store.pmap = pmap_kernel();
	module_map = &module_map_store;

	/* Say hello. */
	banner();

#if NISA > 0 || NPCI > 0
	/* Safe for i/o port / memory space allocation to use malloc now. */
	x86_bus_space_mallocok();
#endif

#ifdef __HAVE_PCPU_AREA
	cpu_pcpuarea_init(&cpu_info_primary);
#endif
	gdt_init();
	x86_64_proc0_pcb_ldt_init();

	cpu_init_tss(&cpu_info_primary);
#if !defined(XENPV)
	ltr(cpu_info_primary.ci_tss_sel);
#endif

	x86_startup();
}

#ifdef XENPV
/* used in assembly */
void hypervisor_callback(void);
void failsafe_callback(void);
void x86_64_switch_context(struct pcb *);
void x86_64_tls_switch(struct lwp *);

void
x86_64_switch_context(struct pcb *new)
{
	HYPERVISOR_stack_switch(GSEL(GDATA_SEL, SEL_KPL), new->pcb_rsp0);
	struct physdev_set_iopl set_iopl;
	set_iopl.iopl = new->pcb_iopl;
	HYPERVISOR_physdev_op(PHYSDEVOP_set_iopl, &set_iopl);
}

void
x86_64_tls_switch(struct lwp *l)
{
	struct cpu_info *ci = curcpu();
	struct pcb *pcb = lwp_getpcb(l);
	struct trapframe *tf = l->l_md.md_regs;
	uint64_t zero = 0;

	/*
	 * Raise the IPL to IPL_HIGH. XXX Still needed?
	 */
	(void)splhigh();

	/* Update segment registers */
	if (pcb->pcb_flags & PCB_COMPAT32) {
		update_descriptor(&ci->ci_gdt[GUFS_SEL], &pcb->pcb_fs);
		update_descriptor(&ci->ci_gdt[GUGS_SEL], &pcb->pcb_gs);
		setds(GSEL(GUDATA32_SEL, SEL_UPL));
		setes(GSEL(GUDATA32_SEL, SEL_UPL));
		setfs(GSEL(GUDATA32_SEL, SEL_UPL));
		HYPERVISOR_set_segment_base(SEGBASE_GS_USER_SEL, tf->tf_gs);
	} else {
		update_descriptor(&ci->ci_gdt[GUFS_SEL], &zero);
		update_descriptor(&ci->ci_gdt[GUGS_SEL], &zero);
		setds(GSEL(GUDATA_SEL, SEL_UPL));
		setes(GSEL(GUDATA_SEL, SEL_UPL));
		setfs(0);
		HYPERVISOR_set_segment_base(SEGBASE_GS_USER_SEL, 0);
		HYPERVISOR_set_segment_base(SEGBASE_FS, pcb->pcb_fs);
		HYPERVISOR_set_segment_base(SEGBASE_GS_USER, pcb->pcb_gs);
	}
}
#endif /* XENPV */

/*
 * Set up proc0's PCB and LDT.
 */
static void
x86_64_proc0_pcb_ldt_init(void)
{
	struct lwp *l = &lwp0;
	struct pcb *pcb = lwp_getpcb(l);

	pcb->pcb_flags = 0;
	pcb->pcb_fs = 0;
	pcb->pcb_gs = 0;
	pcb->pcb_rsp0 = (uvm_lwp_getuarea(l) + USPACE - 16) & ~0xf;
	pcb->pcb_iopl = IOPL_KPL;
	pcb->pcb_dbregs = NULL;
	pcb->pcb_cr0 = rcr0() & ~CR0_TS;
	l->l_md.md_regs = (struct trapframe *)pcb->pcb_rsp0 - 1;

#if !defined(XENPV)
	lldt(GSYSSEL(GLDT_SEL, SEL_KPL));
#else
	xen_set_ldt((vaddr_t)ldtstore, LDT_SIZE >> 3);
	/* Reset TS bit and set kernel stack for interrupt handlers */
	HYPERVISOR_fpu_taskswitch(1);
	HYPERVISOR_stack_switch(GSEL(GDATA_SEL, SEL_KPL), pcb->pcb_rsp0);
	struct physdev_set_iopl set_iopl;
	set_iopl.iopl = pcb->pcb_iopl;
	HYPERVISOR_physdev_op(PHYSDEVOP_set_iopl, &set_iopl);
#endif
}

/*
 * Set up TSS and I/O bitmap.
 */
void
cpu_init_tss(struct cpu_info *ci)
{
#ifdef __HAVE_PCPU_AREA
	const cpuid_t cid = cpu_index(ci);
#endif
	struct cpu_tss *cputss;
	struct nmistore *store;
	uintptr_t p;

#ifdef __HAVE_PCPU_AREA
	cputss = (struct cpu_tss *)&pcpuarea->ent[cid].tss;
#else
	cputss = (struct cpu_tss *)uvm_km_alloc(kernel_map,
	    sizeof(struct cpu_tss), 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
#endif

	cputss->tss.tss_iobase = IOMAP_INVALOFF << 16;

	/* DDB stack */
#ifdef __HAVE_PCPU_AREA
	p = (vaddr_t)&pcpuarea->ent[cid].ist0;
#else
	p = uvm_km_alloc(kernel_map, PAGE_SIZE, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
#endif
	cputss->tss.tss_ist[0] = p + PAGE_SIZE - 16;

	/* double fault */
#ifdef __HAVE_PCPU_AREA
	p = (vaddr_t)&pcpuarea->ent[cid].ist1;
#else
	p = uvm_km_alloc(kernel_map, PAGE_SIZE, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
#endif
	cputss->tss.tss_ist[1] = p + PAGE_SIZE - 16;

	/* NMI - store a structure at the top of the stack */
#ifdef __HAVE_PCPU_AREA
	p = (vaddr_t)&pcpuarea->ent[cid].ist2;
#else
	p = uvm_km_alloc(kernel_map, PAGE_SIZE, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
#endif
	cputss->tss.tss_ist[2] = p + PAGE_SIZE - sizeof(struct nmistore);
	store = (struct nmistore *)(p + PAGE_SIZE - sizeof(struct nmistore));
	store->cr3 = pmap_pdirpa(pmap_kernel(), 0);

	/* DB */
#ifdef __HAVE_PCPU_AREA
	p = (vaddr_t)&pcpuarea->ent[cid].ist3;
#else
	p = uvm_km_alloc(kernel_map, PAGE_SIZE, 0, UVM_KMF_WIRED|UVM_KMF_ZERO);
#endif
	cputss->tss.tss_ist[3] = p + PAGE_SIZE - 16;

	ci->ci_tss = cputss;
	ci->ci_tss_sel = tss_alloc(&cputss->tss);
}

void
buildcontext(struct lwp *l, void *catcher, void *f)
{
	struct trapframe *tf = l->l_md.md_regs;

	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_fs = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_gs = GSEL(GUDATA_SEL, SEL_UPL);

	tf->tf_rip = (uint64_t)catcher;
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_rflags &= ~PSL_CLEARSIG;
	tf->tf_rsp = (uint64_t)f;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);

	/* Ensure FP state is sane */
	fpu_sigreset(l);
}

void
sendsig_sigcontext(const ksiginfo_t *ksi, const sigset_t *mask)
{

	printf("sendsig_sigcontext: illegal\n");
	sigexit(curlwp, SIGILL);
}

void
sendsig_siginfo(const ksiginfo_t *ksi, const sigset_t *mask)
{
	struct lwp *l = curlwp;
	struct proc *p = l->l_proc;
	struct sigacts *ps = p->p_sigacts;
	int onstack, error;
	int sig = ksi->ksi_signo;
	struct sigframe_siginfo *fp, frame;
	sig_t catcher = SIGACTION(p, sig).sa_handler;
	struct trapframe *tf = l->l_md.md_regs;
	char *sp;

	KASSERT(mutex_owned(p->p_lock));

	/* Do we need to jump onto the signal stack? */
	onstack =
	    (l->l_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0 &&
	    (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;

	/* Allocate space for the signal handler context. */
	if (onstack)
		sp = ((char *)l->l_sigstk.ss_sp + l->l_sigstk.ss_size);
	else
		/* AMD64 ABI 128-bytes "red zone". */
		sp = (char *)tf->tf_rsp - 128;

	sp -= sizeof(struct sigframe_siginfo);
	/* Round down the stackpointer to a multiple of 16 for the ABI. */
	fp = (struct sigframe_siginfo *)(((unsigned long)sp & ~15) - 8);

	memset(&frame, 0, sizeof(frame));
	frame.sf_ra = (uint64_t)ps->sa_sigdesc[sig].sd_tramp;
	frame.sf_si._info = ksi->ksi_info;
	frame.sf_uc.uc_flags = _UC_SIGMASK;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_link = l->l_ctxlink;
	frame.sf_uc.uc_flags |= (l->l_sigstk.ss_flags & SS_ONSTACK)
	    ? _UC_SETSTACK : _UC_CLRSTACK;
	sendsig_reset(l, sig);

	mutex_exit(p->p_lock);
	cpu_getmcontext(l, &frame.sf_uc.uc_mcontext, &frame.sf_uc.uc_flags);
	/* Copyout all the fp regs, the signal handler might expect them. */
	error = copyout(&frame, fp, sizeof frame);
	mutex_enter(p->p_lock);

	if (error != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	buildcontext(l, catcher, fp);

	tf->tf_rdi = sig;
	tf->tf_rsi = (uint64_t)&fp->sf_si;
	tf->tf_rdx = tf->tf_r15 = (uint64_t)&fp->sf_uc;

	/* Remember that we're now on the signal stack. */
	if (onstack)
		l->l_sigstk.ss_flags |= SS_ONSTACK;

	if ((vaddr_t)catcher >= VM_MAXUSER_ADDRESS) {
		/*
		 * process has given an invalid address for the
		 * handler. Stop it, but do not do it before so
		 * we can return the right info to userland (or in core dump)
		 */
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}
}

struct pcb dumppcb;

void
cpu_reboot(int howto, char *bootstr)
{
	static bool syncdone = false;
	int s = IPL_NONE;
	__USE(s);	/* ugly otherwise */

	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;

	/* i386 maybe_dump() */

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
	} else {
		if (!db_active)
			suspendsched();
	}

	pmf_system_shutdown(boothowto);

	/* Disable interrupts. */
	s = splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();

haltsys:
	doshutdownhooks();

        if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NACPICA > 0
		if (s != IPL_NONE)
			splx(s);

		acpi_enter_sleep_state(ACPI_STATE_S5);
#endif
#ifdef XEN
		if (vm_guest == VM_GUEST_XENPV ||
		    vm_guest == VM_GUEST_XENPVH ||
		    vm_guest == VM_GUEST_XENPVHVM)
			HYPERVISOR_shutdown();
#endif /* XEN */
	}

	cpu_broadcast_halt();

	if (howto & RB_HALT) {
#if NACPICA > 0
		acpi_disable();
#endif

		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
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
 * XXXfvdl share dumpcode.
 */

/*
 * Perform assorted dump-related initialization tasks.  Assumes that
 * the maximum physical memory address will not increase afterwards.
 */
void
dump_misc_init(void)
{
#ifndef NO_SPARSE_DUMP
	int i;
#endif

	if (dump_headerbuf != NULL)
		return; /* already called */

#ifndef NO_SPARSE_DUMP
	for (i = 0; i < mem_cluster_cnt; ++i) {
		paddr_t top = mem_clusters[i].start + mem_clusters[i].size;
		if (max_paddr < top)
			max_paddr = top;
	}
#ifdef DEBUG
	printf("dump_misc_init: max_paddr = 0x%lx\n",
	    (unsigned long)max_paddr);
#endif
	if (max_paddr == 0) {
		printf("Your machine does not initialize mem_clusters; "
		    "sparse_dumps disabled\n");
		sparse_dump = 0;
	} else {
		sparse_dump_physmap = (void *)uvm_km_alloc(kernel_map,
		    roundup(max_paddr / (PAGE_SIZE * NBBY), PAGE_SIZE),
		    PAGE_SIZE, UVM_KMF_WIRED|UVM_KMF_ZERO);
	}
#endif
	dump_headerbuf = (void *)uvm_km_alloc(kernel_map,
	    dump_headerbuf_size,
	    PAGE_SIZE, UVM_KMF_WIRED|UVM_KMF_ZERO);
	/* XXXjld should check for failure here, disable dumps if so. */
}

#ifndef NO_SPARSE_DUMP
/*
 * Clear the set of pages to include in a sparse dump.
 */
void
sparse_dump_reset(void)
{
	memset(sparse_dump_physmap, 0,
	    roundup(max_paddr / (PAGE_SIZE * NBBY), PAGE_SIZE));
}

/*
 * Include or exclude pages in a sparse dump.
 */
void
sparse_dump_mark(void)
{
	paddr_t p, pstart, pend;
	struct vm_page *pg;
	int i;
	uvm_physseg_t upm;

	/*
	 * Mark all memory pages, then unmark pages that are uninteresting.
	 * Dereferenceing pg->uobject might crash again if another CPU
	 * frees the object out from under us, but we can't lock anything
	 * so it's a risk we have to take.
	 */

	for (i = 0; i < mem_cluster_cnt; ++i) {
		pstart = mem_clusters[i].start / PAGE_SIZE;
		pend = pstart + mem_clusters[i].size / PAGE_SIZE;

		for (p = pstart; p < pend; p++) {
			setbit(sparse_dump_physmap, p);
		}
	}
        for (upm = uvm_physseg_get_first();
	     uvm_physseg_valid_p(upm);
	     upm = uvm_physseg_get_next(upm)) {
		paddr_t pfn;

		/*
		 * We assume that seg->start to seg->end are
		 * uvm_page_physload()ed
		 */
		for (pfn = uvm_physseg_get_start(upm);
		     pfn < uvm_physseg_get_end(upm);
		     pfn++) {
			pg = PHYS_TO_VM_PAGE(ptoa(pfn));

			if (pg->uanon || (pg->flags & PG_FREE) ||
			    (pg->uobject && pg->uobject->pgops)) {
				p = VM_PAGE_TO_PHYS(pg) / PAGE_SIZE;
				clrbit(sparse_dump_physmap, p);
			}
		}
	}
}

/*
 * Machine-dependently decides on the contents of a sparse dump, using
 * the above.
 */
void
cpu_dump_prep_sparse(void)
{
	sparse_dump_reset();
	/* XXX could the alternate recursive page table be skipped? */
	sparse_dump_mark();
	/* Memory for I/O buffers could be unmarked here, for example. */
	/* The kernel text could also be unmarked, but gdb would be upset. */
}
#endif

/*
 * Abstractly iterate over the collection of memory segments to be
 * dumped; the callback lacks the customary environment-pointer
 * argument because none of the current users really need one.
 *
 * To be used only after dump_seg_prep is called to set things up.
 */
int
dump_seg_iter(int (*callback)(paddr_t, paddr_t))
{
	int error, i;

#define CALLBACK(start,size) do {     \
	error = callback(start,size); \
	if (error)                    \
		return error;         \
} while(0)

	for (i = 0; i < mem_cluster_cnt; ++i) {
#ifndef NO_SPARSE_DUMP
		/*
		 * The bitmap is scanned within each memory segment,
		 * rather than over its entire domain, in case any
		 * pages outside of the memory proper have been mapped
		 * into kva; they might be devices that wouldn't
		 * appreciate being arbitrarily read, and including
		 * them could also break the assumption that a sparse
		 * dump will always be smaller than a full one.
		 */
		if (sparse_dump && sparse_dump_physmap) {
			paddr_t p, sp_start, sp_end;
			int lastset;

			sp_start = mem_clusters[i].start;
			sp_end = sp_start + mem_clusters[i].size;
			sp_start = rounddown(sp_start, PAGE_SIZE); /* unnecessary? */
			lastset = 0;
			for (p = sp_start; p < sp_end; p += PAGE_SIZE) {
				int thisset = isset(sparse_dump_physmap,
				    p/PAGE_SIZE);

				if (!lastset && thisset)
					sp_start = p;
				if (lastset && !thisset)
					CALLBACK(sp_start, p - sp_start);
				lastset = thisset;
			}
			if (lastset)
				CALLBACK(sp_start, p - sp_start);
		} else
#endif
			CALLBACK(mem_clusters[i].start, mem_clusters[i].size);
	}
	return 0;
#undef CALLBACK
}

/*
 * Prepare for an impending core dump: decide what's being dumped and
 * how much space it will take up.
 */
void
dump_seg_prep(void)
{
#ifndef NO_SPARSE_DUMP
	if (sparse_dump && sparse_dump_physmap)
		cpu_dump_prep_sparse();
#endif

	dump_nmemsegs = 0;
	dump_npages = 0;
	dump_seg_iter(dump_seg_count_range);

	dump_header_size = ALIGN(sizeof(kcore_seg_t)) +
	    ALIGN(sizeof(cpu_kcore_hdr_t)) +
	    ALIGN(dump_nmemsegs * sizeof(phys_ram_seg_t));
	dump_header_size = roundup(dump_header_size, dbtob(1));

	/*
	 * savecore(8) will read this to decide how many pages to
	 * copy, and cpu_dumpconf has already used the pessimistic
	 * value to set dumplo, so it's time to tell the truth.
	 */
	dumpsize = dump_npages; /* XXX could these just be one variable? */
}

int
dump_seg_count_range(paddr_t start, paddr_t size)
{
	++dump_nmemsegs;
	dump_npages += size / PAGE_SIZE;
	return 0;
}

/*
 * A sparse dump's header may be rather large, due to the number of
 * "segments" emitted.  These routines manage a simple output buffer,
 * so that the header can be written to disk incrementally.
 */
void
dump_header_start(void)
{
	dump_headerbuf_ptr = dump_headerbuf;
	dump_header_blkno = dumplo;
}

int
dump_header_flush(void)
{
	const struct bdevsw *bdev;
	size_t to_write;
	int error;

	bdev = bdevsw_lookup(dumpdev);
	to_write = roundup(dump_headerbuf_ptr - dump_headerbuf, dbtob(1));
	error = bdev->d_dump(dumpdev, dump_header_blkno,
	    dump_headerbuf, to_write);
	dump_header_blkno += btodb(to_write);
	dump_headerbuf_ptr = dump_headerbuf;
	return error;
}

int
dump_header_addbytes(const void* vptr, size_t n)
{
	const char* ptr = vptr;
	int error;

	while (n > dump_headerbuf_avail) {
		memcpy(dump_headerbuf_ptr, ptr, dump_headerbuf_avail);
		ptr += dump_headerbuf_avail;
		n -= dump_headerbuf_avail;
		dump_headerbuf_ptr = dump_headerbuf_end;
		error = dump_header_flush();
		if (error)
			return error;
	}
	memcpy(dump_headerbuf_ptr, ptr, n);
	dump_headerbuf_ptr += n;

	return 0;
}

int
dump_header_addseg(paddr_t start, paddr_t size)
{
	phys_ram_seg_t seg = { start, size };

	return dump_header_addbytes(&seg, sizeof(seg));
}

int
dump_header_finish(void)
{
	memset(dump_headerbuf_ptr, 0, dump_headerbuf_avail);
	return dump_header_flush();
}


/*
 * These variables are needed by /sbin/savecore
 */
uint32_t	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers
 * for a full (non-sparse) dump.
 */
int
cpu_dumpsize(void)
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t)) +
	    ALIGN(mem_cluster_cnt * sizeof(phys_ram_seg_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return (-1);

	return (1);
}

/*
 * cpu_dump_mempagecnt: calculate the size of RAM (in pages) to be dumped
 * for a full (non-sparse) dump.
 */
u_long
cpu_dump_mempagecnt(void)
{
	u_long i, n;

	n = 0;
	for (i = 0; i < mem_cluster_cnt; i++)
		n += atop(mem_clusters[i].size);
	return (n);
}

/*
 * cpu_dump: dump the machine-dependent kernel core dump headers.
 */
int
cpu_dump(void)
{
	kcore_seg_t seg;
	cpu_kcore_hdr_t cpuhdr;
	const struct bdevsw *bdev;

	bdev = bdevsw_lookup(dumpdev);
	if (bdev == NULL)
		return (ENXIO);

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(seg, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	seg.c_size = dump_header_size - ALIGN(sizeof(seg));
	(void)dump_header_addbytes(&seg, ALIGN(sizeof(seg)));

	/*
	 * Add the machine-dependent header info.
	 */
	cpuhdr.ptdpaddr = PDPpaddr;
	cpuhdr.nmemsegs = dump_nmemsegs;
	(void)dump_header_addbytes(&cpuhdr, ALIGN(sizeof(cpuhdr)));

	/*
	 * Write out the memory segment descriptors.
	 */
	return dump_seg_iter(dump_header_addseg);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP  PAGE_SIZE /* must be a multiple of pagesize XXX small */
static vaddr_t dumpspace;

vaddr_t
reserve_dumppages(vaddr_t p)
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

int
dumpsys_seg(paddr_t maddr, paddr_t bytes)
{
	u_long i, m, n;
	daddr_t blkno;
	const struct bdevsw *bdev;
	int (*dump)(dev_t, daddr_t, void *, size_t);
	int error;

	if (dumpdev == NODEV)
		return ENODEV;
	bdev = bdevsw_lookup(dumpdev);
	if (bdev == NULL || bdev->d_psize == NULL)
		return ENODEV;

	dump = bdev->d_dump;

	blkno = dump_header_blkno;
	for (i = 0; i < bytes; i += n, dump_totalbytesleft -= n) {
		/* Print out how many MBs we have left to go. */
		if ((dump_totalbytesleft % (1024*1024)) == 0)
			printf_nolog("%lu ", (unsigned long)
			    (dump_totalbytesleft / (1024 * 1024)));

		/* Limit size for next transfer. */
		n = bytes - i;
		if (n > BYTES_PER_DUMP)
			n = BYTES_PER_DUMP;

		for (m = 0; m < n; m += NBPG)
			pmap_kenter_pa(dumpspace + m, maddr + m,
			    VM_PROT_READ, 0);
		pmap_update(pmap_kernel());

		error = (*dump)(dumpdev, blkno, (void *)dumpspace, n);
		pmap_kremove_local(dumpspace, n);
		if (error)
			return error;
		maddr += n;
		blkno += btodb(n);		/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
		/* operator aborting dump? */
		if (sget() != NULL)
			return EINTR;
#endif
	}
	dump_header_blkno = blkno;

	return 0;
}

void
dodumpsys(void)
{
	const struct bdevsw *bdev;
	int dumpend, psize;
	int error;

	if (dumpdev == NODEV)
		return;

	bdev = bdevsw_lookup(dumpdev);
	if (bdev == NULL || bdev->d_psize == NULL)
		return;
	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();

	printf("\ndumping to dev %llu,%llu (offset=%ld, size=%d):",
	    (unsigned long long)major(dumpdev),
	    (unsigned long long)minor(dumpdev), dumplo, dumpsize);

	if (dumplo <= 0 || dumpsize <= 0) {
		printf(" not possible\n");
		return;
	}

	psize = bdev_size(dumpdev);
	printf("\ndump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

#if 0	/* XXX this doesn't work.  grr. */
	/* toss any characters present prior to dump */
	while (sget() != NULL); /*syscons and pccons differ */
#endif

	dump_seg_prep();
	dumpend = dumplo + btodb(dump_header_size) + ctod(dump_npages);
	if (dumpend > psize) {
		printf("failed: insufficient space (%d < %d)\n",
		    psize, dumpend);
		goto failed;
	}

	dump_header_start();
	if ((error = cpu_dump()) != 0)
		goto err;
	if ((error = dump_header_finish()) != 0)
		goto err;

	if (dump_header_blkno != dumplo + btodb(dump_header_size)) {
		printf("BAD header size (%ld [written] != %ld [expected])\n",
		    (long)(dump_header_blkno - dumplo),
		    (long)btodb(dump_header_size));
		goto failed;
	}

	dump_totalbytesleft = roundup(ptoa(dump_npages), BYTES_PER_DUMP);
	error = dump_seg_iter(dumpsys_seg);

	if (error == 0 && dump_header_blkno != dumpend) {
		printf("BAD dump size (%ld [written] != %ld [expected])\n",
		    (long)(dumpend - dumplo),
		    (long)(dump_header_blkno - dumplo));
		goto failed;
	}

err:
	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
failed:
	printf("\n\n");
	delay(5000000);		/* 5 seconds */
}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 *
 * Sparse dumps can't placed as close to the end as possible, because
 * savecore(8) has to know where to start reading in the dump device
 * before it has access to any of the crashed system's state.
 *
 * Note also that a sparse dump will never be larger than a full one:
 * in order to add a phys_ram_seg_t to the header, at least one page
 * must be removed.
 */
void
cpu_dumpconf(void)
{
	int nblks, dumpblks;	/* size of dump area */

	if (dumpdev == NODEV)
		goto bad;
	nblks = bdev_size(dumpdev);
	if (nblks <= ctod(1))
		goto bad;

	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		goto bad;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = cpu_dump_mempagecnt();

	dumpblks += ctod(dumpsize);

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1))) {
#ifndef NO_SPARSE_DUMP
		/* A sparse dump might (and hopefully will) fit. */
		dumplo = ctod(1);
#else
		/* But if we're not configured for that, punt. */
		goto bad;
#endif
	} else {
		/* Put dump at end of partition */
		dumplo = nblks - dumpblks;
	}


	/* Now that we've decided this will work, init ancillary stuff. */
	dump_misc_init();
	return;

 bad:
	dumpsize = 0;
}

/*
 * Clear registers on exec
 */
void
setregs(struct lwp *l, struct exec_package *pack, vaddr_t stack)
{
	struct pcb *pcb = lwp_getpcb(l);
	struct trapframe *tf;

#ifdef USER_LDT
	pmap_ldt_cleanup(l);
#endif

	fpu_clear(l, pack->ep_osversion >= 699002600
	    ? __NetBSD_NPXCW__ : __NetBSD_COMPAT_NPXCW__);
	x86_dbregs_clear(l);

	kpreempt_disable();
	pcb->pcb_flags = 0;
	l->l_proc->p_flag &= ~PK_32;
	l->l_md.md_flags = MDL_IRET;
	cpu_segregs64_zero(l);
	kpreempt_enable();

	tf = l->l_md.md_regs;
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_rdi = 0;
	tf->tf_rsi = 0;
	tf->tf_rbp = 0;
	tf->tf_rbx = l->l_proc->p_psstrp;
	tf->tf_rdx = 0;
	tf->tf_rcx = 0;
	tf->tf_rax = 0;
	tf->tf_rip = pack->ep_entry;
	tf->tf_cs = LSEL(LUCODE_SEL, SEL_UPL);
	tf->tf_rflags = PSL_USERSET;
	tf->tf_rsp = stack;
	tf->tf_ss = LSEL(LUDATA_SEL, SEL_UPL);
}

/*
 * Initialize segments and descriptor tables
 */
char *ldtstore;
char *gdtstore;

void
setgate(struct gate_descriptor *gd, void *func,
    int ist, int type, int dpl, int sel)
{
	vaddr_t vaddr;

	vaddr = ((vaddr_t)gd) & ~PAGE_MASK;

	kpreempt_disable();
	pmap_changeprot_local(vaddr, VM_PROT_READ|VM_PROT_WRITE);

	gd->gd_looffset = (uint64_t)func & 0xffff;
	gd->gd_selector = sel;
	gd->gd_ist = ist;
	gd->gd_type = type;
	gd->gd_dpl = dpl;
	gd->gd_p = 1;
	gd->gd_hioffset = (uint64_t)func >> 16;
	gd->gd_zero = 0;
	gd->gd_xx1 = 0;
	gd->gd_xx2 = 0;
	gd->gd_xx3 = 0;

	pmap_changeprot_local(vaddr, VM_PROT_READ);
	kpreempt_enable();
}

void
unsetgate(struct gate_descriptor *gd)
{
	vaddr_t vaddr;

	vaddr = ((vaddr_t)gd) & ~PAGE_MASK;

	kpreempt_disable();
	pmap_changeprot_local(vaddr, VM_PROT_READ|VM_PROT_WRITE);

	memset(gd, 0, sizeof (*gd));

	pmap_changeprot_local(vaddr, VM_PROT_READ);
	kpreempt_enable();
}

void
setregion(struct region_descriptor *rd, void *base, uint16_t limit)
{
	rd->rd_limit = limit;
	rd->rd_base = (uint64_t)base;
}

/*
 * Note that the base and limit fields are ignored in long mode.
 */
void
set_mem_segment(struct mem_segment_descriptor *sd, void *base, size_t limit,
	int type, int dpl, int gran, int def32, int is64)
{
	sd->sd_lolimit = (unsigned)limit;
	sd->sd_lobase = (unsigned long)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (unsigned)limit >> 16;
	sd->sd_avl = 0;
	sd->sd_long = is64;
	sd->sd_def32 = def32;
	sd->sd_gran = gran;
	sd->sd_hibase = (unsigned long)base >> 24;
}

void
set_sys_segment(struct sys_segment_descriptor *sd, void *base, size_t limit,
	int type, int dpl, int gran)
{
	memset(sd, 0, sizeof *sd);
	sd->sd_lolimit = (unsigned)limit;
	sd->sd_lobase = (uint64_t)base;
	sd->sd_type = type;
	sd->sd_dpl = dpl;
	sd->sd_p = 1;
	sd->sd_hilimit = (unsigned)limit >> 16;
	sd->sd_gran = gran;
	sd->sd_hibase = (uint64_t)base >> 24;
}

void
cpu_init_idt(struct cpu_info *ci)
{
	struct region_descriptor region;
	idt_descriptor_t *idt;

	idt = ci->ci_idtvec.iv_idt;
	setregion(&region, idt, NIDT * sizeof(idt[0]) - 1);
	lidt(&region);
}

#define	IDTVEC(name)	__CONCAT(X, name)
typedef void (vector)(void);
extern vector IDTVEC(syscall);
extern vector IDTVEC(syscall32);
extern vector IDTVEC(osyscall);
extern vector *x86_exceptions[];

#ifndef XENPV
static void
init_x86_64_ksyms(void)
{
#if NKSYMS || defined(DDB) || defined(MODULAR)
	extern int end;
	extern int *esym;
	struct btinfo_symtab *symtab;
	vaddr_t tssym, tesym;

#ifdef DDB
	db_machine_init();
#endif

	symtab = lookup_bootinfo(BTINFO_SYMTAB);
	if (symtab) {
#ifdef KASLR
		tssym = bootspace.head.va;
		tesym = bootspace.head.va; /* (unused...) */
#else
		tssym = (vaddr_t)symtab->ssym + KERNBASE;
		tesym = (vaddr_t)symtab->esym + KERNBASE;
#endif
		ksyms_addsyms_elf(symtab->nsym, (void *)tssym, (void *)tesym);
	} else {
		uintptr_t endp = (uintptr_t)(void *)&end;

		if (vm_guest == VM_GUEST_GENPVH)
			ksyms_addsyms_elf(0, ((long *)endp) + 1, esym);
		else
			ksyms_addsyms_elf(*(long *)endp, ((long *)endp) + 1, esym);
	}
#endif
}
#endif /* XENPV */

void __noasan
init_bootspace(void)
{
	extern char __rodata_start;
	extern char __data_start;
	extern char __kernel_end;
	size_t i = 0;

	memset(&bootspace, 0, sizeof(bootspace));

	bootspace.head.va = KERNTEXTOFF;
	bootspace.head.pa = KERNTEXTOFF - KERNBASE;
	bootspace.head.sz = 0;

	bootspace.segs[i].type = BTSEG_TEXT;
	bootspace.segs[i].va = KERNTEXTOFF;
	bootspace.segs[i].pa = KERNTEXTOFF - KERNBASE;
	bootspace.segs[i].sz = (size_t)&__rodata_start - KERNTEXTOFF;
	i++;

	bootspace.segs[i].type = BTSEG_RODATA;
	bootspace.segs[i].va = (vaddr_t)&__rodata_start;
	bootspace.segs[i].pa = (paddr_t)&__rodata_start - KERNBASE;
	bootspace.segs[i].sz = (size_t)&__data_start - (size_t)&__rodata_start;
	i++;

	bootspace.segs[i].type = BTSEG_DATA;
	bootspace.segs[i].va = (vaddr_t)&__data_start;
	bootspace.segs[i].pa = (paddr_t)&__data_start - KERNBASE;
	bootspace.segs[i].sz = (size_t)&__kernel_end - (size_t)&__data_start;
	i++;

	bootspace.boot.va = (vaddr_t)&__kernel_end;
	bootspace.boot.pa = (paddr_t)&__kernel_end - KERNBASE;
	bootspace.boot.sz = (size_t)(atdevbase + IOM_SIZE) -
	    (size_t)&__kernel_end;

	/* In locore.S, we allocated a tmp va. We will use it now. */
	bootspace.spareva = KERNBASE + NKL2_KIMG_ENTRIES * NBPD_L2;

	/* Virtual address of the L4 page. */
	bootspace.pdir = (vaddr_t)(PDPpaddr + KERNBASE);

	/* Kernel module map. */
	bootspace.smodule = (vaddr_t)atdevbase + IOM_SIZE;
	bootspace.emodule = KERNBASE + NKL2_KIMG_ENTRIES * NBPD_L2;
}

static void
init_pte(void)
{
#ifndef XENPV
	extern uint32_t nox_flag;
	pd_entry_t *pdir = (pd_entry_t *)bootspace.pdir;
	pdir[L4_SLOT_PTE] = PDPpaddr | PTE_W | ((uint64_t)nox_flag << 32) |
	    PTE_P;
#endif

	extern pd_entry_t *normal_pdes[3];
	normal_pdes[0] = L2_BASE;
	normal_pdes[1] = L3_BASE;
	normal_pdes[2] = L4_BASE;
}

void
init_slotspace(void)
{
	/*
	 * XXX Too early to use cprng(9), or even entropy_extract.
	 */
	struct entpool pool;
	size_t randhole;
	vaddr_t randva;
	uint64_t sample;
	vaddr_t va;

	memset(&pool, 0, sizeof pool);
	cpu_rng_early_sample(&sample);
	entpool_enter(&pool, &sample, sizeof sample);

	memset(&slotspace, 0, sizeof(slotspace));

	/* User. [256, because we want to land in >= 256] */
	slotspace.area[SLAREA_USER].sslot = 0;
	slotspace.area[SLAREA_USER].nslot = PDIR_SLOT_USERLIM+1;
	slotspace.area[SLAREA_USER].active = true;

#ifdef XENPV
	/* PTE. */
	slotspace.area[SLAREA_PTE].sslot = PDIR_SLOT_PTE;
	slotspace.area[SLAREA_PTE].nslot = 1;
	slotspace.area[SLAREA_PTE].active = true;
#endif

#ifdef __HAVE_PCPU_AREA
	/* Per-CPU. */
	slotspace.area[SLAREA_PCPU].sslot = PDIR_SLOT_PCPU;
	slotspace.area[SLAREA_PCPU].nslot = 1;
	slotspace.area[SLAREA_PCPU].active = true;
#endif

#ifdef __HAVE_DIRECT_MAP
	/* Direct Map. [Randomized later] */
	slotspace.area[SLAREA_DMAP].active = false;
#endif

#ifdef XENPV
	/* Hypervisor. */
	slotspace.area[SLAREA_HYPV].sslot = 256;
	slotspace.area[SLAREA_HYPV].nslot = 17;
	slotspace.area[SLAREA_HYPV].active = true;
#endif

#ifdef KASAN
	/* ASAN. */
	slotspace.area[SLAREA_ASAN].sslot = L4_SLOT_KASAN;
	slotspace.area[SLAREA_ASAN].nslot = NL4_SLOT_KASAN;
	slotspace.area[SLAREA_ASAN].active = true;
#endif

#ifdef KMSAN
	/* MSAN. */
	slotspace.area[SLAREA_MSAN].sslot = L4_SLOT_KMSAN;
	slotspace.area[SLAREA_MSAN].nslot = NL4_SLOT_KMSAN;
	slotspace.area[SLAREA_MSAN].active = true;
#endif

	/* Kernel. */
	slotspace.area[SLAREA_KERN].sslot = L4_SLOT_KERNBASE;
	slotspace.area[SLAREA_KERN].nslot = 1;
	slotspace.area[SLAREA_KERN].active = true;

	/* Main. */
	cpu_rng_early_sample(&sample);
	entpool_enter(&pool, &sample, sizeof sample);
	entpool_extract(&pool, &randhole, sizeof randhole);
	entpool_extract(&pool, &randva, sizeof randva);
	va = slotspace_rand(SLAREA_MAIN, NKL4_MAX_ENTRIES * NBPD_L4,
	    NBPD_L4, randhole, randva); /* TODO: NBPD_L1 */
	vm_min_kernel_address = va;
	vm_max_kernel_address = va + NKL4_MAX_ENTRIES * NBPD_L4;

#ifndef XENPV
	/* PTE. */
	cpu_rng_early_sample(&sample);
	entpool_enter(&pool, &sample, sizeof sample);
	entpool_extract(&pool, &randhole, sizeof randhole);
	entpool_extract(&pool, &randva, sizeof randva);
	va = slotspace_rand(SLAREA_PTE, NBPD_L4, NBPD_L4, randhole, randva);
	pte_base = (pd_entry_t *)va;
#endif

	explicit_memset(&pool, 0, sizeof pool);
}

void
init_x86_64(paddr_t first_avail)
{
	extern void consinit(void);
	struct region_descriptor region;
	struct mem_segment_descriptor *ldt_segp;
	struct idt_vec *iv;
	idt_descriptor_t *idt;
	int x;
	struct pcb *pcb;
	extern vaddr_t lwp0uarea;
#ifndef XENPV
	extern paddr_t local_apic_pa;
#endif

	KASSERT(first_avail % PAGE_SIZE == 0);

#ifdef XENPV
	KASSERT(HYPERVISOR_shared_info != NULL);
	cpu_info_primary.ci_vcpu = &HYPERVISOR_shared_info->vcpu_info[0];
#endif

#ifdef XEN
	if (vm_guest == VM_GUEST_XENPVH || vm_guest == VM_GUEST_GENPVH)
		xen_parse_cmdline(XEN_PARSE_BOOTFLAGS, NULL);
#endif
	init_pte();

	uvm_lwp_setuarea(&lwp0, lwp0uarea);

	cpu_probe(&cpu_info_primary);
#ifdef SVS
	svs_init();
#endif
	cpu_init_msrs(&cpu_info_primary, true);
#ifndef XENPV
	cpu_speculation_init(&cpu_info_primary);
#endif

	use_pae = 1; /* PAE always enabled in long mode */

	pcb = lwp_getpcb(&lwp0);
#ifdef XENPV
	mutex_init(&pte_lock, MUTEX_DEFAULT, IPL_VM);
	pcb->pcb_cr3 = xen_start_info.pt_base - KERNBASE;
#else
	pcb->pcb_cr3 = PDPpaddr;
#endif

#if NISA > 0 || NPCI > 0
	x86_bus_space_init();
#endif

	pat_init(&cpu_info_primary);

	consinit();	/* XXX SHOULD NOT BE DONE HERE */

	/*
	 * Initialize RNG to get entropy ASAP either from CPU
	 * RDRAND/RDSEED or from seed on disk.  Must happen after
	 * cpu_init_msrs.  Prefer to happen after consinit so we have
	 * the opportunity to print useful feedback.
	 */
	cpu_rng_init();
	x86_rndseed();

	/*
	 * Initialize PAGE_SIZE-dependent variables.
	 */
	uvm_md_init();

	uvmexp.ncolors = 2;

	avail_start = first_avail;

#ifndef XENPV
	/*
	 * Low memory reservations:
	 * Page 0:	BIOS data
	 * Page 1:	BIOS callback (not used yet, for symmetry with i386)
	 * Page 2:	MP bootstrap code (MP_TRAMPOLINE)
	 * Page 3:	ACPI wakeup code (ACPI_WAKEUP_ADDR)
	 * Page 4:	Temporary page table for 0MB-4MB
	 * Page 5:	Temporary page directory
	 * Page 6:	Temporary page map level 3
	 * Page 7:	Temporary page map level 4
	 */
	lowmem_rsvd = 8 * PAGE_SIZE;

	/* Initialize the memory clusters (needed in pmap_bootstrap). */
	init_x86_clusters();
#else
	/* Parse Xen command line (replace bootinfo) */
	xen_parse_cmdline(XEN_PARSE_BOOTFLAGS, NULL);

	avail_end = ctob(xen_start_info.nr_pages);
	pmap_pa_start = (KERNTEXTOFF - KERNBASE);
	pmap_pa_end = avail_end;
#endif

	/*
	 * Call pmap initialization to make new kernel address space.
	 * We must do this before loading pages into the VM system.
	 */
	pmap_bootstrap(VM_MIN_KERNEL_ADDRESS);

#ifndef XENPV
	/* Internalize the physical pages into the VM system. */
	init_x86_vm(avail_start);
#else
	physmem = xen_start_info.nr_pages;
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end), VM_FREELIST_DEFAULT);
#endif

	init_x86_msgbuf();

	kasan_init();
	kcsan_init();
	kmsan_init((void *)lwp0uarea);

	pmap_growkernel(VM_MIN_KERNEL_ADDRESS + 32 * 1024 * 1024);

	kpreempt_disable();

#ifndef XENPV
	pmap_kenter_pa(local_apic_va, local_apic_pa,
	    VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_update(pmap_kernel());
	memset((void *)local_apic_va, 0, PAGE_SIZE);
#endif

	pmap_kenter_pa(idt_vaddr, idt_paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_kenter_pa(gdt_vaddr, gdt_paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_kenter_pa(ldt_vaddr, ldt_paddr, VM_PROT_READ|VM_PROT_WRITE, 0);
	pmap_update(pmap_kernel());
	memset((void *)idt_vaddr, 0, PAGE_SIZE);
	memset((void *)gdt_vaddr, 0, PAGE_SIZE);
	memset((void *)ldt_vaddr, 0, PAGE_SIZE);

#ifndef XENPV
	pmap_changeprot_local(idt_vaddr, VM_PROT_READ);
#endif

	pmap_update(pmap_kernel());

	iv = &(cpu_info_primary.ci_idtvec);
	idt_vec_init_cpu_md(iv, cpu_index(&cpu_info_primary));
	idt = iv->iv_idt;
	gdtstore = (char *)gdt_vaddr;
	ldtstore = (char *)ldt_vaddr;

	/*
	 * Make GDT gates and memory segments.
	 */
	set_mem_segment(GDT_ADDR_MEM(gdtstore, GCODE_SEL), 0,
	    0xfffff, SDT_MEMERA, SEL_KPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GDATA_SEL), 0,
	    0xfffff, SDT_MEMRWA, SEL_KPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUCODE_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMERA, SEL_UPL, 1, 0, 1);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUDATA_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS) - 1, SDT_MEMRWA, SEL_UPL, 1, 0, 1);

#ifndef XENPV
	set_sys_segment(GDT_ADDR_SYS(gdtstore, GLDT_SEL), ldtstore,
	    LDT_SIZE - 1, SDT_SYSLDT, SEL_KPL, 0);
#endif

	/*
	 * Make LDT memory segments.
	 */
	*(struct mem_segment_descriptor *)(ldtstore + LUCODE_SEL) =
	    *GDT_ADDR_MEM(gdtstore, GUCODE_SEL);
	*(struct mem_segment_descriptor *)(ldtstore + LUDATA_SEL) =
	    *GDT_ADDR_MEM(gdtstore, GUDATA_SEL);

	/*
	 * 32 bit GDT entries.
	 */
	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUCODE32_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS32) - 1, SDT_MEMERA, SEL_UPL, 1, 1, 0);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUDATA32_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS32) - 1, SDT_MEMRWA, SEL_UPL, 1, 1, 0);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUFS_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS32) - 1, SDT_MEMRWA, SEL_UPL, 1, 1, 0);

	set_mem_segment(GDT_ADDR_MEM(gdtstore, GUGS_SEL), 0,
	    x86_btop(VM_MAXUSER_ADDRESS32) - 1, SDT_MEMRWA, SEL_UPL, 1, 1, 0);

	/*
	 * 32 bit LDT entries.
	 */
	ldt_segp = (struct mem_segment_descriptor *)(ldtstore + LUCODE32_SEL);
	set_mem_segment(ldt_segp, 0, x86_btop(VM_MAXUSER_ADDRESS32) - 1,
	    SDT_MEMERA, SEL_UPL, 1, 1, 0);
	ldt_segp = (struct mem_segment_descriptor *)(ldtstore + LUDATA32_SEL);
	set_mem_segment(ldt_segp, 0, x86_btop(VM_MAXUSER_ADDRESS32) - 1,
	    SDT_MEMRWA, SEL_UPL, 1, 1, 0);

	/* CPU-specific IDT exceptions. */
	for (x = 0; x < NCPUIDT; x++) {
		int sel, ist;

		/* Reset to default. Special cases below */
		sel = SEL_KPL;
		ist = 0;

		idt_vec_reserve(iv, x);

		switch (x) {
		case 1:	/* DB */
			ist = 4;
			break;
		case 2:	/* NMI */
			ist = 3;
			break;
		case 3:
		case 4:			
			sel = SEL_UPL;
			break;
		case 8:	/* double fault */
			ist = 2;
			break;
#ifdef XENPV			
		case 18: /* MCA */
			sel |= 0x4; /* Auto EOI/mask */
			break;
#endif /* XENPV */			
		default:
			break;
		}

		set_idtgate(&idt[x], x86_exceptions[x], ist, SDT_SYS386IGT,
		    sel, GSEL(GCODE_SEL, SEL_KPL));
	}

	/* new-style interrupt gate for syscalls */
	idt_vec_reserve(iv, 128);
	set_idtgate(&idt[128], &IDTVEC(osyscall), 0, SDT_SYS386IGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	kpreempt_enable();

	setregion(&region, gdtstore, DYNSEL_START - 1);
	lgdt(&region);

#ifdef XENPV
	/* Init Xen callbacks and syscall handlers */
	if (HYPERVISOR_set_callbacks(
	    (unsigned long) hypervisor_callback,
	    (unsigned long) failsafe_callback,
	    (unsigned long) Xsyscall))
		panic("HYPERVISOR_set_callbacks() failed");
#endif /* XENPV */

	cpu_init_idt(&cpu_info_primary);

#ifdef XENPV
	xen_init_ksyms();
#else /* XENPV */
#ifdef XEN
	if (vm_guest == VM_GUEST_XENPVH)
		xen_init_ksyms();
	else
#endif /* XEN */
		init_x86_64_ksyms();
#endif /* XENPV */

#ifndef XENPV
	intr_default_setup();
#else
	events_default_setup();
#endif

	splraise(IPL_HIGH);
	x86_enable_intr();

#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif
#ifdef KGDB
	kgdb_port_init();
	if (boothowto & RB_KDB) {
		kgdb_debug_init = 1;
		kgdb_connect(1);
	}
#endif

	pcb->pcb_dbregs = NULL;
	x86_dbregs_init();
}

void
cpu_reset(void)
{
#ifndef XENPV
	idt_descriptor_t *idt;
	vaddr_t vaddr;

	idt = cpu_info_primary.ci_idtvec.iv_idt;
	vaddr = (vaddr_t)idt;
#endif

	x86_disable_intr();

#ifdef XENPV
	HYPERVISOR_reboot();
#else

	x86_reset();

	/*
	 * Try to cause a triple fault and watchdog reset by making the IDT
	 * invalid and causing a fault.
	 */
	kpreempt_disable();
	pmap_changeprot_local(vaddr, VM_PROT_READ|VM_PROT_WRITE);
	memset((void *)idt, 0, NIDT * sizeof(idt[0]));
	kpreempt_enable();
	breakpoint();

#if 0
	/*
	 * Try to cause a triple fault and watchdog reset by unmapping the
	 * entire address space and doing a TLB flush.
	 */
	memset((void *)PTD, 0, PAGE_SIZE);
	tlbflush();
#endif
#endif	/* XENPV */

	for (;;);
}

void
cpu_getmcontext(struct lwp *l, mcontext_t *mcp, unsigned int *flags)
{
	const struct trapframe *tf = l->l_md.md_regs;
	__greg_t ras_rip;

	mcp->__gregs[_REG_RDI] = tf->tf_rdi;
	mcp->__gregs[_REG_RSI] = tf->tf_rsi;
	mcp->__gregs[_REG_RDX] = tf->tf_rdx;
	mcp->__gregs[_REG_R10] = tf->tf_r10;
	mcp->__gregs[_REG_R8]  = tf->tf_r8;
	mcp->__gregs[_REG_R9]  = tf->tf_r9;
	/* argX not touched */
	mcp->__gregs[_REG_RCX] = tf->tf_rcx;
	mcp->__gregs[_REG_R11] = tf->tf_r11;
	mcp->__gregs[_REG_R12] = tf->tf_r12;
	mcp->__gregs[_REG_R13] = tf->tf_r13;
	mcp->__gregs[_REG_R14] = tf->tf_r14;
	mcp->__gregs[_REG_R15] = tf->tf_r15;
	mcp->__gregs[_REG_RBP] = tf->tf_rbp;
	mcp->__gregs[_REG_RBX] = tf->tf_rbx;
	mcp->__gregs[_REG_RAX] = tf->tf_rax;
	mcp->__gregs[_REG_GS]  = 0;
	mcp->__gregs[_REG_FS]  = 0;
	mcp->__gregs[_REG_ES]  = GSEL(GUDATA_SEL, SEL_UPL);
	mcp->__gregs[_REG_DS]  = GSEL(GUDATA_SEL, SEL_UPL);
	mcp->__gregs[_REG_TRAPNO] = tf->tf_trapno;
	mcp->__gregs[_REG_ERR] = tf->tf_err;
	mcp->__gregs[_REG_RIP] = tf->tf_rip;
	mcp->__gregs[_REG_CS]  = LSEL(LUCODE_SEL, SEL_UPL);
	mcp->__gregs[_REG_RFLAGS] = tf->tf_rflags;
	mcp->__gregs[_REG_RSP] = tf->tf_rsp;
	mcp->__gregs[_REG_SS]  = LSEL(LUDATA_SEL, SEL_UPL);

	if ((ras_rip = (__greg_t)ras_lookup(l->l_proc,
	    (void *) mcp->__gregs[_REG_RIP])) != -1)
		mcp->__gregs[_REG_RIP] = ras_rip;

	*flags |= _UC_CPU;

	mcp->_mc_tlsbase = (uintptr_t)l->l_private;
	*flags |= _UC_TLSBASE;

	process_read_fpregs_xmm(l, (struct fxsave *)&mcp->__fpregs);
	*flags |= _UC_FPU;
}

int
cpu_setmcontext(struct lwp *l, const mcontext_t *mcp, unsigned int flags)
{
	struct trapframe *tf = l->l_md.md_regs;
	const __greg_t *gr = mcp->__gregs;
	struct proc *p = l->l_proc;
	int error;
	int64_t rflags;

	CTASSERT(sizeof (mcontext_t) == 26 * 8 + 8 + 512);

	if ((flags & _UC_CPU) != 0) {
		error = cpu_mcontext_validate(l, mcp);
		if (error != 0)
			return error;

		tf->tf_rdi  = gr[_REG_RDI];
		tf->tf_rsi  = gr[_REG_RSI];
		tf->tf_rdx  = gr[_REG_RDX];
		tf->tf_r10  = gr[_REG_R10];
		tf->tf_r8   = gr[_REG_R8];
		tf->tf_r9   = gr[_REG_R9];
		/* argX not touched */
		tf->tf_rcx  = gr[_REG_RCX];
		tf->tf_r11  = gr[_REG_R11];
		tf->tf_r12  = gr[_REG_R12];
		tf->tf_r13  = gr[_REG_R13];
		tf->tf_r14  = gr[_REG_R14];
		tf->tf_r15  = gr[_REG_R15];
		tf->tf_rbp  = gr[_REG_RBP];
		tf->tf_rbx  = gr[_REG_RBX];
		tf->tf_rax  = gr[_REG_RAX];
		tf->tf_gs   = 0;
		tf->tf_fs   = 0;
		tf->tf_es   = GSEL(GUDATA_SEL, SEL_UPL);
		tf->tf_ds   = GSEL(GUDATA_SEL, SEL_UPL);
		/* trapno, err not touched */
		tf->tf_rip  = gr[_REG_RIP];
		tf->tf_cs   = LSEL(LUCODE_SEL, SEL_UPL);
		rflags = tf->tf_rflags;
		rflags &= ~PSL_USER;
		tf->tf_rflags = rflags | (gr[_REG_RFLAGS] & PSL_USER);
		tf->tf_rsp  = gr[_REG_RSP];
		tf->tf_ss   = LSEL(LUDATA_SEL, SEL_UPL);

		l->l_md.md_flags |= MDL_IRET;
	}

	if ((flags & _UC_FPU) != 0)
		process_write_fpregs_xmm(l, (const struct fxsave *)&mcp->__fpregs);

	if ((flags & _UC_TLSBASE) != 0)
		lwp_setprivate(l, (void *)(uintptr_t)mcp->_mc_tlsbase);

	mutex_enter(p->p_lock);
	if (flags & _UC_SETSTACK)
		l->l_sigstk.ss_flags |= SS_ONSTACK;
	if (flags & _UC_CLRSTACK)
		l->l_sigstk.ss_flags &= ~SS_ONSTACK;
	mutex_exit(p->p_lock);

	return 0;
}

int
cpu_mcontext_validate(struct lwp *l, const mcontext_t *mcp)
{
	struct proc *p __diagused = l->l_proc;
	struct trapframe *tf = l->l_md.md_regs;
	const __greg_t *gr;
	uint16_t sel;

	KASSERT((p->p_flag & PK_32) == 0);
	gr = mcp->__gregs;

	if (((gr[_REG_RFLAGS] ^ tf->tf_rflags) & PSL_USERSTATIC) != 0)
		return EINVAL;

	sel = gr[_REG_ES] & 0xffff;
	if (sel != 0 && !VALID_USER_DSEL(sel))
		return EINVAL;

	sel = gr[_REG_FS] & 0xffff;
	if (sel != 0 && !VALID_USER_DSEL(sel))
		return EINVAL;

	sel = gr[_REG_GS] & 0xffff;
	if (sel != 0 && !VALID_USER_DSEL(sel))
		return EINVAL;

	sel = gr[_REG_DS] & 0xffff;
	if (!VALID_USER_DSEL(sel))
		return EINVAL;

#ifndef XENPV
	sel = gr[_REG_SS] & 0xffff;
	if (!VALID_USER_DSEL(sel))
		return EINVAL;

	sel = gr[_REG_CS] & 0xffff;
	if (!VALID_USER_CSEL(sel))
		return EINVAL;
#endif

	if (gr[_REG_RIP] >= VM_MAXUSER_ADDRESS)
		return EINVAL;

	return 0;
}

int
mm_md_kernacc(void *ptr, vm_prot_t prot, bool *handled)
{
	const vaddr_t v = (vaddr_t)ptr;
	vaddr_t kva, kva_end;
	size_t i;

	kva = bootspace.head.va;
	kva_end = kva + bootspace.head.sz;
	if (v >= kva && v < kva_end) {
		*handled = true;
		return 0;
	}

	for (i = 0; i < BTSPACE_NSEGS; i++) {
		kva = bootspace.segs[i].va;
		kva_end = kva + bootspace.segs[i].sz;
		if (v < kva || v >= kva_end)
			continue;
		*handled = true;
		if (bootspace.segs[i].type == BTSEG_TEXT ||
		    bootspace.segs[i].type == BTSEG_RODATA) {
			if (prot & VM_PROT_WRITE) {
				return EFAULT;
			}
		}
		return 0;
	}

	kva = bootspace.boot.va;
	kva_end = kva + bootspace.boot.sz;
	if (v >= kva && v < kva_end) {
		*handled = true;
		return 0;
	}

	if (v >= bootspace.smodule && v < bootspace.emodule) {
		*handled = true;
		if (!uvm_map_checkprot(module_map, v, v + 1, prot)) {
			return EFAULT;
		}
	} else {
		*handled = false;
	}
	return 0;
}

/*
 * Zero out a 64bit LWP's segments registers. Used when exec'ing a new
 * 64bit program.
 */
void
cpu_segregs64_zero(struct lwp *l)
{
	struct trapframe * const tf = l->l_md.md_regs;
	struct pcb *pcb;
	uint64_t zero = 0;

	KASSERT(kpreempt_disabled());
	KASSERT((l->l_proc->p_flag & PK_32) == 0);
	KASSERT(l == curlwp);

	pcb = lwp_getpcb(l);

	tf->tf_fs = 0;
	tf->tf_gs = 0;
	setds(GSEL(GUDATA_SEL, SEL_UPL));
	setes(GSEL(GUDATA_SEL, SEL_UPL));
	setfs(0);
	setusergs(0);

#ifndef XENPV
	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_KERNELGSBASE, 0);
#else
	HYPERVISOR_set_segment_base(SEGBASE_FS, 0);
	HYPERVISOR_set_segment_base(SEGBASE_GS_USER, 0);
#endif

	pcb->pcb_fs = 0;
	pcb->pcb_gs = 0;
	update_descriptor(&curcpu()->ci_gdt[GUFS_SEL], &zero);
	update_descriptor(&curcpu()->ci_gdt[GUGS_SEL], &zero);
}

/*
 * Zero out a 32bit LWP's segments registers. Used when exec'ing a new
 * 32bit program.
 */
void
cpu_segregs32_zero(struct lwp *l)
{
	struct trapframe * const tf = l->l_md.md_regs;
	struct pcb *pcb;
	uint64_t zero = 0;

	KASSERT(kpreempt_disabled());
	KASSERT(l->l_proc->p_flag & PK_32);
	KASSERT(l == curlwp);

	pcb = lwp_getpcb(l);

	tf->tf_fs = 0;
	tf->tf_gs = 0;
	setds(GSEL(GUDATA32_SEL, SEL_UPL));
	setes(GSEL(GUDATA32_SEL, SEL_UPL));
	setfs(0);
	setusergs(0);
	pcb->pcb_fs = 0;
	pcb->pcb_gs = 0;
	update_descriptor(&curcpu()->ci_gdt[GUFS_SEL], &zero);
	update_descriptor(&curcpu()->ci_gdt[GUGS_SEL], &zero);
}

/*
 * Load an LWP's TLS context, possibly changing the %fs and %gs selectors.
 * Used only for 32-bit processes.
 */
void
cpu_fsgs_reload(struct lwp *l, int fssel, int gssel)
{
	struct trapframe *tf;
	struct pcb *pcb;

	KASSERT(l->l_proc->p_flag & PK_32);
	KASSERT(l == curlwp);

	tf = l->l_md.md_regs;
	fssel &= 0xFFFF;
	gssel &= 0xFFFF;

	pcb = lwp_getpcb(l);
	kpreempt_disable();
	update_descriptor(&curcpu()->ci_gdt[GUFS_SEL], &pcb->pcb_fs);
	update_descriptor(&curcpu()->ci_gdt[GUGS_SEL], &pcb->pcb_gs);

#ifdef XENPV
	setusergs(gssel);
#endif

	tf->tf_fs = fssel;
	tf->tf_gs = gssel;
	kpreempt_enable();
}

bool
mm_md_direct_mapped_io(void *addr, paddr_t *paddr)
{
	vaddr_t va = (vaddr_t)addr;

#ifdef __HAVE_DIRECT_MAP
	if (va >= PMAP_DIRECT_BASE && va < PMAP_DIRECT_END) {
		*paddr = PMAP_DIRECT_UNMAP(va);
		return true;
	}
#else
	__USE(va);
#endif

	return false;
}

bool
mm_md_direct_mapped_phys(paddr_t paddr, vaddr_t *vaddr)
{
#ifdef __HAVE_DIRECT_MAP
	*vaddr = PMAP_DIRECT_MAP(paddr);
	return true;
#else
	return false;
#endif
}

static void
idt_vec_copy(struct idt_vec *dst, struct idt_vec *src)
{
	idt_descriptor_t *idt_dst;

	idt_dst = dst->iv_idt;

	kpreempt_disable();
	pmap_changeprot_local((vaddr_t)idt_dst, VM_PROT_READ|VM_PROT_WRITE);

	memcpy(idt_dst, src->iv_idt, PAGE_SIZE);
	memcpy(dst->iv_allocmap, src->iv_allocmap, sizeof(dst->iv_allocmap));

	pmap_changeprot_local((vaddr_t)idt_dst, VM_PROT_READ);
	kpreempt_enable();
}

void
idt_vec_init_cpu_md(struct idt_vec *iv, cpuid_t cid)
{
	vaddr_t va;

	if (cid != cpu_index(&cpu_info_primary) &&
	    idt_vec_is_pcpu()) {
#ifdef __HAVE_PCPU_AREA
		va = (vaddr_t)&pcpuarea->ent[cid].idt;
#else
		struct vm_page *pg;

		va = uvm_km_alloc(kernel_map, PAGE_SIZE, 0,
		    UVM_KMF_VAONLY);
		pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_ZERO);
		if (pg == NULL) {
			panic("failed to allocate a page for IDT");
		}
		pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg),
		    VM_PROT_READ|VM_PROT_WRITE, 0);
		pmap_update(pmap_kernel());
#endif

		memset((void *)va, 0, PAGE_SIZE);
#ifndef XENPV
		pmap_changeprot_local(va, VM_PROT_READ);
#endif
		pmap_update(pmap_kernel());

		iv->iv_idt = (void *)va;
		idt_vec_copy(iv, &(cpu_info_primary.ci_idtvec));
	} else {
		iv->iv_idt = (void *)idt_vaddr;
	}
}
