/*	$NetBSD: vm86.c,v 1.51 2009/11/21 03:11:00 rmind Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John T. Kohl and Charles M. Hannum.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vm86.c,v 1.51 2009/11/21 03:11:00 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/syscallargs.h>
#include <sys/ktrace.h>

#include <machine/sysarch.h>
#include <machine/vm86.h>

static void fast_intxx(struct lwp *, int);
static inline int is_bitset(int, void *);

#define	CS(tf)		(*(u_short *)&tf->tf_cs)
#define	IP(tf)		(*(u_short *)&tf->tf_eip)
#define	SS(tf)		(*(u_short *)&tf->tf_ss)
#define	SP(tf)		(*(u_short *)&tf->tf_esp)

#define putword(base, ptr, val) \
	do { \
		ptr = (ptr - 1) & 0xffff; \
		subyte((void *)(base+ptr), (val>>8)); \
	        ptr = (ptr - 1) & 0xffff; \
		subyte((void *)(base+ptr), (val&0xff)); \
	} while (0)

#define putdword(base, ptr, val) \
	do { \
		putword(base, ptr, (val >> 16)); \
		putword(base, ptr, (val & 0xffff)); \
	} while (0)

#define getbyte(base, ptr, byte) \
	do { \
		u_long tmplong = fubyte((void *)(base+ptr)); \
		if (tmplong == ~0) goto bad; \
		ptr = (ptr + 1) & 0xffff; \
		byte = tmplong; \
	} while (0)

#define getword(base, ptr, word) \
	do { \
		u_long b1, b2; \
		getbyte(base, ptr, b1); \
		getbyte(base, ptr, b2); \
		word = b1 | b2 << 8; \
	} while (0)

#define getdword(base, ptr, dword) \
	do { \
		u_long w1, w2; \
		getword(base, ptr, w1); \
		getword(base, ptr, w2); \
		dword = w1 | w2 << 16; \
	} while (0)

static inline int
is_bitset(int nr, void *bitmap)
{
	u_int byte;		/* bt instruction doesn't do
					   bytes--it examines ints! */
	bitmap = (char *)bitmap + (nr / NBBY);
	nr = nr % NBBY;
	byte = fubyte(bitmap);

	__asm volatile("btl %2,%1\n\tsbbl %0,%0"
			     :"=r" (nr)
			     :"r" (byte),"r" (nr));
	return (nr);
}


#define V86_AH(regs)	(((u_char *)&((regs)->tf_eax))[1])
#define V86_AL(regs)	(((u_char *)&((regs)->tf_eax))[0])

static void
fast_intxx(struct lwp *l, int intrno)
{
	struct trapframe *tf = l->l_md.md_regs;
	struct pcb *pcb;
	/*
	 * handle certain interrupts directly by pushing the interrupt
	 * frame and resetting registers, but only if user said that's ok
	 * (i.e. not revectored.)  Otherwise bump to 32-bit user handler.
	 */
	struct vm86_struct *u_vm86p;
	struct { u_short ip, cs; } ihand;

	u_long ss, sp;

	/* 
	 * Note: u_vm86p points to user-space, we only compute offsets
	 * and don't deref it. is_revectored() above does fubyte() to
	 * get stuff from it
	 */
	pcb = lwp_getpcb(l);
	u_vm86p = (struct vm86_struct *)pcb->vm86_userp;

	/* 
	 * If user requested special handling, return to user space with
	 * indication of which INT was requested.
	 */
	if (is_bitset(intrno, &u_vm86p->int_byuser[0]))
		goto vector;

	/*
	 * If it's interrupt 0x21 (special in the DOS world) and the
	 * sub-command (in AH) was requested for special handling,
	 * return to user mode.
	 */
	if (intrno == 0x21 && is_bitset(V86_AH(tf), &u_vm86p->int21_byuser[0]))
		goto vector;

	/*
	 * Fetch intr handler info from "real-mode" IDT based at addr 0 in
	 * the user address space.
	 */
	if (copyin((void *)(intrno * sizeof(ihand)), &ihand, sizeof(ihand))) {
		/*
		 * No IDT!  What Linux does here is simply call back into
		 * userspace with the VM86_INTx arg as if it was a revectored
		 * int.  Some applications rely on this (i.e. dynamically
		 * emulate an IDT), and those that don't will crash in a
		 * spectacular way, I suppose.
		 *	--thorpej@NetBSD.org
		 */
		goto vector;
	}

	/*
	 * Otherwise, push flags, cs, eip, and jump to handler to
	 * simulate direct INT call.
	 */
	ss = SS(tf) << 4;
	sp = SP(tf);

	putword(ss, sp, get_vflags_short(l));
	putword(ss, sp, CS(tf));
	putword(ss, sp, IP(tf));
	SP(tf) = sp;

	IP(tf) = ihand.ip;
	CS(tf) = ihand.cs;

	return;

vector:
	vm86_return(l, VM86_MAKEVAL(VM86_INTx, intrno));
	return;
}

void
vm86_return(struct lwp *l, int retval)
{
	struct proc *p = l->l_proc;
	ksiginfo_t ksi;

	mutex_enter(p->p_lock);

	/*
	 * We can't set the virtual flags in our real trap frame,
	 * since it's used to jump to the signal handler.  Instead we
	 * let sendsig() pull in the vm86_eflags bits.
	 */
	if (sigismember(&l->l_sigmask, SIGURG)) {
#ifdef DIAGNOSTIC
		printf("pid %d killed on VM86 protocol screwup (SIGURG blocked)\n",
		    p->p_pid);
#endif
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	if (sigismember(&p->p_sigctx.ps_sigignore, SIGURG)) {
#ifdef DIAGNOSTIC
		printf("pid %d killed on VM86 protocol screwup (SIGURG ignored)\n",
		    p->p_pid);
#endif
		sigexit(l, SIGILL);
		/* NOTREACHED */
	}

	mutex_exit(p->p_lock);

	KSI_INIT_TRAP(&ksi);
	ksi.ksi_signo = SIGURG;
	ksi.ksi_trap = retval;
	(*p->p_emul->e_trapsignal)(l, &ksi);
}

#define	CLI	0xFA
#define	STI	0xFB
#define	INTxx	0xCD
#define	INTO	0xCE
#define	IRET	0xCF
#define	OPSIZ	0x66
#define	INT3	0xCC	/* Actually the process gets 32-bit IDT to handle it */
#define	LOCK	0xF0
#define	PUSHF	0x9C
#define	POPF	0x9D

/*
 * Handle a GP fault that occurred while in VM86 mode.  Things that are easy
 * to handle here are done here (much more efficient than trapping to 32-bit
 * handler code and then having it restart VM86 mode).
 */
void
vm86_gpfault(struct lwp *l, int type)
{
	struct trapframe *tf = l->l_md.md_regs;
	struct proc *p = l->l_proc;
	/*
	 * we want to fetch some stuff from the current user virtual
	 * address space for checking.  remember that the frame's
	 * segment selectors are real-mode style selectors.
	 */
	u_long cs, ip, ss, sp;
	u_char tmpbyte;
	u_long tmpword, tmpdword;
	int trace;

	cs = CS(tf) << 4;
	ip = IP(tf);
	ss = SS(tf) << 4;
	sp = SP(tf);

	trace = tf->tf_eflags & PSL_T;

	/*
	 * For most of these, we must set all the registers before calling
	 * macros/functions which might do a vm86_return.
	 */
	getbyte(cs, ip, tmpbyte);
	IP(tf) = ip;
	switch (tmpbyte) {
	case CLI:
		/* simulate handling of IF */
		clr_vif(l);
		break;

	case STI:
		/* simulate handling of IF.
		 * XXX the i386 enables interrupts one instruction later.
		 * code here is wrong, but much simpler than doing it Right.
		 */
		set_vif(l);
		break;

	case INTxx:
		/* try fast intxx, or return to 32bit mode to handle it. */
		getbyte(cs, ip, tmpbyte);
		IP(tf) = ip;
		fast_intxx(l, tmpbyte);
		break;

	case INTO:
		if (tf->tf_eflags & PSL_V)
			fast_intxx(l, 4);
		break;

	case PUSHF:
		putword(ss, sp, get_vflags_short(l));
		SP(tf) = sp;
		break;

	case IRET:
		getword(ss, sp, IP(tf));
		getword(ss, sp, CS(tf));
	case POPF:
		getword(ss, sp, tmpword);
		set_vflags_short(l, tmpword);
		SP(tf) = sp;
		break;

	case OPSIZ:
		getbyte(cs, ip, tmpbyte);
		IP(tf) = ip;
		switch (tmpbyte) {
		case PUSHF:
			putdword(ss, sp, get_vflags(l) & ~PSL_VM);
			SP(tf) = sp;
			break;

		case IRET:
			getdword(ss, sp, IP(tf));
			getdword(ss, sp, CS(tf));
		case POPF:
			getdword(ss, sp, tmpdword);
			set_vflags(l, tmpdword | PSL_VM);
			SP(tf) = sp;
			break;

		default:
			IP(tf) -= 2;
			goto bad;
		}
		break;

	case LOCK:
	default:
		IP(tf) -= 1;
		goto bad;
	}

	if (trace && tf->tf_eflags & PSL_VM) {
		ksiginfo_t ksi;

		KSI_INIT_TRAP(&ksi);
		ksi.ksi_signo = SIGTRAP;
		ksi.ksi_code = TRAP_TRACE;
		ksi.ksi_trap = T_TRCTRAP;
		ksi.ksi_addr = (void *)tf->tf_eip;
		(*p->p_emul->e_trapsignal)(l, &ksi);
	}
	return;

bad:
	vm86_return(l, VM86_UNKNOWN);
	return;
}

int
x86_vm86(struct lwp *l, char *args, register_t *retval)
{
	struct trapframe *tf = l->l_md.md_regs;
	struct pcb *pcb = lwp_getpcb(l);
	struct vm86_kern vm86s;
	struct proc *p;
	int error;

	error = copyin(args, &vm86s, sizeof(vm86s));
	if (error)
		return (error);

	pcb->vm86_userp = (void *)args;

	/*
	 * Keep mask of flags we simulate to simulate a particular type of
	 * processor.
	 */
	switch (vm86s.ss_cpu_type) {
	case VCPU_086:
	case VCPU_186:
	case VCPU_286:
		pcb->vm86_flagmask = PSL_ID|PSL_AC|PSL_NT|PSL_IOPL;
		break;
	case VCPU_386:
		pcb->vm86_flagmask = PSL_ID|PSL_AC;
		break;
	case VCPU_486:
		pcb->vm86_flagmask = PSL_ID;
		break;
	case VCPU_586:
		pcb->vm86_flagmask = 0;
		break;
	default:
		return (EINVAL);
	}

#define DOVREG(reg,REG) tf->tf_vm86_##reg = (u_short) vm86s.regs[_REG_##REG]
#define DOREG(reg,REG) tf->tf_##reg = (u_short) vm86s.regs[_REG_##REG]

	DOVREG(ds,GS);
	DOVREG(es,ES);
	DOVREG(fs,FS);
	DOVREG(gs,GS);
	DOREG(edi,EDI);
	DOREG(esi,ESI);
	DOREG(ebp,EBP);
	DOREG(eax,EAX);
	DOREG(ebx,EBX);
	DOREG(ecx,ECX);
	DOREG(edx,EDX);
	DOREG(eip,EIP);
	DOREG(cs,CS);
	DOREG(esp,ESP);
	DOREG(ss,SS);

#undef	DOVREG
#undef	DOREG

	/* Going into vm86 mode jumps off the signal stack. */
	p = l->l_proc;
	mutex_enter(p->p_lock);
	l->l_sigstk.ss_flags &= ~SS_ONSTACK;
	mutex_exit(p->p_lock);

	set_vflags(l, vm86s.regs[_REG_EFL] | PSL_VM);

	return (EJUSTRETURN);
}

struct compat_16_vm86_kern {
	struct sigcontext regs;
	unsigned long ss_cpu_type;
};

struct compat_16_vm86_struct {
	struct compat_16_vm86_kern substr;
	unsigned long screen_bitmap;	/* not used/supported (yet) */
	unsigned long flags;		/* not used/supported (yet) */
	unsigned char int_byuser[32];	/* 256 bits each: pass control to user */
	unsigned char int21_byuser[32];	/* otherwise, handle directly */
};

int
compat_16_x86_vm86(struct lwp *l, char *args, register_t *retval)
{
	struct trapframe *tf = l->l_md.md_regs;
	struct pcb *pcb = lwp_getpcb(l);
	struct compat_16_vm86_kern vm86s;
	struct proc *p = l->l_proc;
	int error;

	error = copyin(args, &vm86s, sizeof(vm86s));
	if (error)
		return (error);

	pcb->vm86_userp = (void *)(args +
	    (offsetof(struct compat_16_vm86_struct, screen_bitmap)
	    - offsetof(struct vm86_struct, screen_bitmap)));
	printf("offsetting by %lu\n", (unsigned long)
	    (offsetof(struct compat_16_vm86_struct, screen_bitmap)
	    - offsetof(struct vm86_struct, screen_bitmap)));

	/*
	 * Keep mask of flags we simulate to simulate a particular type of
	 * processor.
	 */
	switch (vm86s.ss_cpu_type) {
	case VCPU_086:
	case VCPU_186:
	case VCPU_286:
		pcb->vm86_flagmask = PSL_ID|PSL_AC|PSL_NT|PSL_IOPL;
		break;
	case VCPU_386:
		pcb->vm86_flagmask = PSL_ID|PSL_AC;
		break;
	case VCPU_486:
		pcb->vm86_flagmask = PSL_ID;
		break;
	case VCPU_586:
		pcb->vm86_flagmask = 0;
		break;
	default:
		return (EINVAL);
	}

#define DOVREG(reg) tf->tf_vm86_##reg = (u_short) vm86s.regs.sc_##reg
#define DOREG(reg) tf->tf_##reg = (u_short) vm86s.regs.sc_##reg

	DOVREG(ds);
	DOVREG(es);
	DOVREG(fs);
	DOVREG(gs);
	DOREG(edi);
	DOREG(esi);
	DOREG(ebp);
	DOREG(eax);
	DOREG(ebx);
	DOREG(ecx);
	DOREG(edx);
	DOREG(eip);
	DOREG(cs);
	DOREG(esp);
	DOREG(ss);

#undef	DOVREG
#undef	DOREG

	mutex_enter(p->p_lock);

	/* Going into vm86 mode jumps off the signal stack. */
	l->l_sigstk.ss_flags &= ~SS_ONSTACK;

	mutex_exit(p->p_lock);

	set_vflags(l, vm86s.regs.sc_eflags | PSL_VM);

	return (EJUSTRETURN);
}
