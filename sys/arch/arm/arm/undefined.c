/*	$NetBSD: undefined.c,v 1.56 2015/04/15 13:22:50 matt Exp $	*/

/*
 * Copyright (c) 2001 Ben Harris.
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * undefined.c
 *
 * Fault handler
 *
 * Created      : 06/01/95
 */

#define FAST_FPE

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_dtrace.h"

#include <sys/param.h>
#ifdef KGDB
#include <sys/kgdb.h>
#endif

__KERNEL_RCSID(0, "$NetBSD: undefined.c,v 1.56 2015/04/15 13:22:50 matt Exp $");

#include <sys/kmem.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/vmmeter.h>
#include <sys/cpu.h>
#ifdef FAST_FPE
#include <sys/acct.h>
#endif
#include <sys/userret.h>

#include <uvm/uvm_extern.h>

#include <arm/locore.h>
#include <arm/undefined.h>

#include <machine/pcb.h>
#include <machine/trap.h>

#include <arch/arm/arm/disassem.h>

#ifdef DDB
#include <ddb/db_output.h>
#include <machine/db_machdep.h>
#endif

#ifdef acorn26
#include <machine/machdep.h>
#endif

static int gdb_trapper(u_int, u_int, struct trapframe *, int);

LIST_HEAD(, undefined_handler) undefined_handlers[NUM_UNKNOWN_HANDLERS];


void *
install_coproc_handler(int coproc, undef_handler_t handler)
{
	struct undefined_handler *uh;

	KASSERT(coproc >= 0 && coproc < NUM_UNKNOWN_HANDLERS);
	KASSERT(handler != NULL); /* Used to be legal. */

	uh = kmem_alloc(sizeof(*uh), KM_NOSLEEP);
	KASSERT(uh != NULL);
	uh->uh_handler = handler;
	install_coproc_handler_static(coproc, uh);
	return uh;
}

void
install_coproc_handler_static(int coproc, struct undefined_handler *uh)
{

	LIST_INSERT_HEAD(&undefined_handlers[coproc], uh, uh_link);
}

void
remove_coproc_handler(void *cookie)
{
	struct undefined_handler *uh = cookie;

	LIST_REMOVE(uh, uh_link);
	kmem_free(uh, sizeof(*uh));
}

static int
cp15_trapper(u_int addr, u_int insn, struct trapframe *tf, int code)
{
	struct lwp * const l = curlwp;

#if defined(THUMB_CODE) && !defined(CPU_ARMV7)
	if (tf->tf_spsr & PSR_T_bit)
		return 1;
#endif
	if (code != FAULT_USER)
		return 1;

	/*
	 * Don't overwrite sp, pc, etc.
	 */
	const u_int regno = (insn >> 12) & 15;
	if (regno > 12)
		return 1;

	/*
	 * Get a pointer to the register used in the instruction to be emulated.
	 */
	register_t * const regp = &tf->tf_r0 + regno;

	/*
	 * Handle MRC p15, 0, <Rd>, c13, c0, 3 (Read User read-only thread id)
	 */
	if ((insn & 0xffff0fff) == 0xee1d0f70) {
		*regp = (uintptr_t)l->l_private;
		tf->tf_pc += INSN_SIZE;
		curcpu()->ci_und_cp15_ev.ev_count++;
		return 0;
	}

	/*
	 * Handle {MRC,MCR} p15, 0, <Rd>, c13, c0, 2 (User read/write thread id)
	 */
	if ((insn & 0xffef0fff) == 0xee0d0f50) {
		struct pcb * const pcb = lwp_getpcb(l);
		if (insn & 0x00100000)
			*regp = pcb->pcb_user_pid_rw;
		else
			pcb->pcb_user_pid_rw = *regp;
		tf->tf_pc += INSN_SIZE;
		curcpu()->ci_und_cp15_ev.ev_count++;
		return 0;
	}

	return 1;
}

static int
gdb_trapper(u_int addr, u_int insn, struct trapframe *tf, int code)
{
	struct lwp * const l = curlwp;

#ifdef THUMB_CODE
	if (tf->tf_spsr & PSR_T_bit) {
		if (insn == GDB_THUMB_BREAKPOINT)
			goto bkpt;
	}
	else
#endif
	{
		if (insn == GDB_BREAKPOINT || insn == GDB5_BREAKPOINT) {
#ifdef THUMB_CODE
		bkpt:
#endif
			if (code == FAULT_USER) {
				ksiginfo_t ksi;

				KSI_INIT_TRAP(&ksi);
				ksi.ksi_signo = SIGTRAP;
				ksi.ksi_code = TRAP_BRKPT;
				ksi.ksi_addr = (uint32_t *)addr;
				ksi.ksi_trap = 0;
				trapsignal(l, &ksi);
				return 0;
			}
#ifdef KGDB
			return !kgdb_trap(T_BREAKPOINT, tf);
#endif
		}
	}
	return 1;
}

static struct undefined_handler cp15_uh;
static struct undefined_handler gdb_uh;
#ifdef THUMB_CODE
static struct undefined_handler gdb_uh_thumb;
#endif

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>

/* Not used for now, but needed for dtrace/fbt modules */
dtrace_doubletrap_func_t	dtrace_doubletrap_func = NULL;
dtrace_trap_func_t		dtrace_trap_func = NULL;

int (* dtrace_invop_jump_addr)(uintptr_t, uintptr_t *, uintptr_t);
void (* dtrace_emulation_jump_addr)(int, struct trapframe *);

static int
dtrace_trapper(u_int addr, struct trapframe *frame)
{
	int op;
	struct trapframe back;
	u_int insn = read_insn(addr, false);

	if (dtrace_invop_jump_addr == NULL || dtrace_emulation_jump_addr == NULL)
		return 1;

	if (!DTRACE_IS_BREAKPOINT(insn))
		return 1;

	/* cond value is encoded in the first byte */
	if (!arm_cond_ok_p(__SHIFTIN(insn, INSN_COND_MASK), frame->tf_spsr)) {
		frame->tf_pc += INSN_SIZE;
		return 0;
	}

	back = *frame;
	op = dtrace_invop_jump_addr(addr, (uintptr_t *) frame->tf_svc_sp, frame->tf_r0);
	*frame = back;

	dtrace_emulation_jump_addr(op, frame);

	return 0;
}
#endif

void
undefined_init(void)
{
	int loop;

	/* Not actually necessary -- the initialiser is just NULL */
	for (loop = 0; loop < NUM_UNKNOWN_HANDLERS; ++loop)
		LIST_INIT(&undefined_handlers[loop]);

	/* Install handler for CP15 emulation */
	cp15_uh.uh_handler = cp15_trapper;
	install_coproc_handler_static(SYSTEM_COPROC, &cp15_uh);

	/* Install handler for GDB breakpoints */
	gdb_uh.uh_handler = gdb_trapper;
	install_coproc_handler_static(CORE_UNKNOWN_HANDLER, &gdb_uh);
#ifdef THUMB_CODE
	gdb_uh_thumb.uh_handler = gdb_trapper;
	install_coproc_handler_static(THUMB_UNKNOWN_HANDLER, &gdb_uh_thumb);
#endif
}

void
undefinedinstruction(trapframe_t *tf)
{
	struct lwp *l;
	vaddr_t fault_pc;
	int fault_instruction;
	int fault_code;
	int coprocessor;
	int user;
	struct undefined_handler *uh;
#ifdef VERBOSE_ARM32
	int s;
#endif

	curcpu()->ci_und_ev.ev_count++;

#ifdef KDTRACE_HOOKS
	if ((tf->tf_spsr & PSR_MODE) != PSR_USR32_MODE) {
		tf->tf_pc -= INSN_SIZE;
		if (dtrace_trapper(tf->tf_pc, tf) == 0)
			return;
		tf->tf_pc += INSN_SIZE; /* Reset for the rest code */
	}
#endif

	/* Enable interrupts if they were enabled before the exception. */
#ifdef acorn26
	if ((tf->tf_r15 & R15_IRQ_DISABLE) == 0)
		int_on();
#else
	restore_interrupts(tf->tf_spsr & IF32_bits);
#endif

#ifndef acorn26
#ifdef THUMB_CODE
	if (tf->tf_spsr & PSR_T_bit)
		tf->tf_pc -= THUMB_INSN_SIZE;
	else
#endif
	{
		tf->tf_pc -= INSN_SIZE;
	}
#endif

#ifdef __PROG26
	fault_pc = tf->tf_r15 & R15_PC;
#else
	fault_pc = tf->tf_pc;
#endif

	/* Get the current lwp/proc structure or lwp0/proc0 if there is none. */
	l = curlwp;

#ifdef __PROG26
	if ((tf->tf_r15 & R15_MODE) == R15_MODE_USR) {
#else
	if ((tf->tf_spsr & PSR_MODE) == PSR_USR32_MODE) {
#endif
		user = 1;
		LWP_CACHE_CREDS(l, l->l_proc);
	} else
		user = 0;


#ifdef THUMB_CODE
	if (tf->tf_spsr & PSR_T_bit) {
		fault_instruction = read_thumb_insn(fault_pc, user);
		if (fault_instruction >= 0xe000) {
			fault_instruction = (fault_instruction << 16)
			    | read_thumb_insn(fault_pc + 2, user);
		}
	}
	else
#endif
	{
		/*
		 * Make sure the program counter is correctly aligned so we
		 * don't take an alignment fault trying to read the opcode.
		 */
		if (__predict_false((fault_pc & 3) != 0)) {
			ksiginfo_t ksi;
			/* Give the user an illegal instruction signal. */
			KSI_INIT_TRAP(&ksi);
			ksi.ksi_signo = SIGILL;
			ksi.ksi_code = ILL_ILLOPC;
			ksi.ksi_addr = (uint32_t *)(intptr_t) fault_pc;
			trapsignal(l, &ksi);
			userret(l);
			return;
		}
	 	/*
		 * Should use fuword() here .. but in the interests of
		 * squeezing every  bit of speed we will just use
		 * ReadWord(). We know the instruction can be read
		 * as was just executed so this will never fail unless
		 * the kernel is screwed up in which case it does
		 * not really matter does it ?
		 */
		fault_instruction = read_insn(fault_pc, user);
	}

	/* Update vmmeter statistics */
	curcpu()->ci_data.cpu_ntrap++;

#ifdef THUMB_CODE
	if ((tf->tf_spsr & PSR_T_bit) && !CPU_IS_ARMV7_P()) {
		coprocessor = THUMB_UNKNOWN_HANDLER;
	}
	else
#endif
	{
		/* Check for coprocessor instruction */

		/*
		 * According to the datasheets you only need to look at
		 * bit 27 of the instruction to tell the difference
		 * between and undefined instruction and a coprocessor
		 * instruction following an undefined instruction trap.
		 *
		 * ARMv5 adds undefined instructions in the NV space,
		 * even when bit 27 is set.
		 */

		if ((fault_instruction & (1 << 27)) != 0
		    && (fault_instruction & 0xf0000000) != 0xf0000000) {
			coprocessor = (fault_instruction >> 8) & 0x0f;
#ifdef THUMB_CODE
		} else if ((tf->tf_spsr & PSR_T_bit) && !CPU_IS_ARMV7_P()) {
			coprocessor = THUMB_UNKNOWN_HANDLER;
#endif
		} else {
			coprocessor = CORE_UNKNOWN_HANDLER;
		}
	}

	if (user) {
		/*
		 * Modify the fault_code to reflect the USR/SVC state at
		 * time of fault.
		 */
		fault_code = FAULT_USER;
		lwp_settrapframe(l, tf);
	} else
		fault_code = 0;

	/* OK this is were we do something about the instruction. */
	LIST_FOREACH(uh, &undefined_handlers[coprocessor], uh_link)
	    if (uh->uh_handler(fault_pc, fault_instruction, tf,
			       fault_code) == 0)
		    break;

	if (uh == NULL) {
		/* Fault has not been handled */
		ksiginfo_t ksi; 
		
#ifdef VERBOSE_ARM32
		s = spltty();

		if ((fault_instruction & 0x0f000010) == 0x0e000000) {
			printf("CDP\n");
			disassemble(fault_pc);
		} else if ((fault_instruction & 0x0e000000) == 0x0c000000) {
			printf("LDC/STC\n");
			disassemble(fault_pc);
		} else if ((fault_instruction & 0x0f000010) == 0x0e000010) {
			printf("MRC/MCR\n");
			disassemble(fault_pc);
		} else if ((fault_instruction & ~INSN_COND_MASK)
			 != (KERNEL_BREAKPOINT & ~INSN_COND_MASK)) {
			printf("Undefined instruction\n");
			disassemble(fault_pc);
		}

		splx(s);
#endif
        
		if ((fault_code & FAULT_USER) == 0) {
#ifdef DDB
			db_printf("Undefined instruction %#x in kernel at %#lx (LR %#x SP %#x)\n",
			    fault_instruction, fault_pc, tf->tf_svc_lr, tf->tf_svc_sp);
			kdb_trap(T_FAULT, tf);
#else
			panic("undefined instruction %#x in kernel at %#lx", fault_instruction, fault_pc);
#endif
		}
		KSI_INIT_TRAP(&ksi);
		ksi.ksi_signo = SIGILL;
		ksi.ksi_code = ILL_ILLOPC;
		ksi.ksi_addr = (uint32_t *)fault_pc;
		ksi.ksi_trap = fault_instruction;
		trapsignal(l, &ksi);
	}

	if ((fault_code & FAULT_USER) == 0)
		return;

	userret(l);
}
