/*	$NetBSD: db_trace.c,v 1.31 2015/01/24 15:44:32 skrll Exp $	*/

/*
 * Copyright (c) 2000, 2001 Ben Harris
 * Copyright (c) 1996 Scott K. Stevens
 *
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

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: db_trace.c,v 1.31 2015/01/24 15:44:32 skrll Exp $");

#include <sys/proc.h>
#include <arm/armreg.h>
#include <arm/cpufunc.h>
#include <arm/pcb.h>
#include <machine/db_machdep.h>
#include <machine/vmparam.h>

#include <ddb/db_access.h>
#include <ddb/db_interface.h>
#include <ddb/db_sym.h>
#include <ddb/db_proc.h>
#include <ddb/db_output.h>

#define INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

/*
 * APCS stack frames are awkward beasts, so I don't think even trying to use
 * a structure to represent them is a good idea.
 *
 * Here's the diagram from the APCS.  Increasing address is _up_ the page.
 *
 *          save code pointer       [fp]        <- fp points to here
 *          return link value       [fp, #-4]
 *          return sp value         [fp, #-8]
 *          return fp value         [fp, #-12]
 *          [saved v7 value]
 *          [saved v6 value]
 *          [saved v5 value]
 *          [saved v4 value]
 *          [saved v3 value]
 *          [saved v2 value]
 *          [saved v1 value]
 *          [saved a4 value]
 *          [saved a3 value]
 *          [saved a2 value]
 *          [saved a1 value]
 *
 * The save code pointer points twelve bytes beyond the start of the
 * code sequence (usually a single STM) that created the stack frame.
 * We have to disassemble it if we want to know which of the optional
 * fields are actually present.
 */

#define FR_SCP	(0)
#define FR_RLV	(-1)
#define FR_RSP	(-2)
#define FR_RFP	(-3)

void
db_stack_trace_print(db_expr_t addr, bool have_addr,
		db_expr_t count, const char *modif,
		void (*pr)(const char *, ...))
{
	uint32_t	*frame, *lastframe;
	const char	*cp = modif;
	char c;
	bool		kernel_only = true;
	bool		trace_thread = false;
	bool		trace_full = false;
	bool		lwpaddr = false;
	db_addr_t	scp, pc;
	int		scp_offset;

	while ((c = *cp++) != 0) {
		if (c == 'a') {
			lwpaddr = true;
			trace_thread = true;
		}
		if (c == 'u')
			kernel_only = false;
		if (c == 't')
			trace_thread = true;
		if (c == 'f')
			trace_full = true;
	}

#ifdef _KERNEL
	if (!have_addr)
		frame = (uint32_t *)(DDB_REGS->tf_r11);
	else
#endif
	{
		if (trace_thread) {
			struct pcb *pcb;
			proc_t p;
			lwp_t l;

			if (lwpaddr) {
				db_read_bytes(addr, sizeof(l),
				    (char *)&l);
				db_read_bytes((db_addr_t)l.l_proc,
				    sizeof(p), (char *)&p);
				(*pr)("trace: pid %d ", p.p_pid);
			} else {
				proc_t	*pp;

				(*pr)("trace: pid %d ", (int)addr);
				if ((pp = db_proc_find((pid_t)addr)) == 0) {
					(*pr)("not found\n");
					return;
				}
				db_read_bytes((db_addr_t)pp, sizeof(p), (char *)&p);
				addr = (db_addr_t)p.p_lwps.lh_first;
				db_read_bytes(addr, sizeof(l), (char *)&l);
			}
			(*pr)("lid %d ", l.l_lid);
			pcb = lwp_getpcb(&l);
#ifndef _KERNEL
			struct pcb pcbb;
			db_read_bytes((db_addr_t)pcb, sizeof(*pcb),
			    (char *)&pcbb);
			pcb = &pcbb;
#endif
#ifdef acorn26
			frame = (uint32_t *)(pcb->pcb_sf->sf_r11);
#else
			frame = (uint32_t *)(pcb->pcb_un.un_32.pcb32_r11);
#endif
			(*pr)("at %p\n", frame);
		} else
			frame = (uint32_t *)(addr);
	}
	scp_offset = -(get_pc_str_offset() >> 2);

	if (frame == NULL)
		return;

	lastframe = frame;
#ifndef _KERNEL
	uint32_t frameb[4];
	db_read_bytes((db_addr_t)(frame - 3), sizeof(frameb),
	    (char *)frameb);
	frame = frameb + 3;
#endif

	/*
	 * In theory, the SCP isn't guaranteed to be in the function
	 * that generated the stack frame.  We hope for the best.
	 */
#ifdef __PROG26
	scp = frame[FR_SCP] & R15_PC;
#else
	scp = frame[FR_SCP];
#endif
	pc = scp;

	while (count--) {
		uint32_t	savecode;
		int		r;
		uint32_t	*rp;
		const char	*sep;

#ifdef __PROG26
		scp = frame[FR_SCP] & R15_PC;
#else
		scp = frame[FR_SCP];
#endif
		(*pr)("%p: ", lastframe);

		db_printsym(pc, DB_STGY_PROC, pr);
		if (trace_full) {
			(*pr)("\n\t");
#ifdef __PROG26
			(*pr)("pc =0x%08x rlv=0x%08x (", pc,
			     frame[FR_RLV] & R15_PC);
			db_printsym(frame[FR_RLV] & R15_PC, DB_STGY_PROC, pr);
			(*pr)(")\n");
#else
			(*pr)("pc =0x%08x rlv=0x%08x (", pc, frame[FR_RLV]);
			db_printsym(frame[FR_RLV], DB_STGY_PROC, pr);
			(*pr)(")\n");
#endif
			(*pr)("\trsp=0x%08x rfp=0x%08x", frame[FR_RSP],
			     frame[FR_RFP]);
		}

#ifndef _KERNEL
		db_read_bytes((db_addr_t)((uint32_t *)scp + scp_offset),
		    sizeof(savecode), (void *)&savecode);
#else
		if ((scp & 3) == 0) {
			savecode = ((uint32_t *)scp)[scp_offset];
		} else {
			savecode = 0;
		}
#endif
		if (trace_full &&
		    (savecode & 0x0e100000) == 0x08000000) {
			/* Looks like an STM */
			rp = frame - 4;
			sep = "\n\t";
			for (r = 10; r >= 0; r--) {
				if (savecode & (1 << r)) {
					(*pr)("%sr%d=0x%08x",
					    sep, r, *rp--);
					sep = (frame - rp) % 4 == 2 ?
					    "\n\t" : " ";
				}
			}
		}

		(*pr)("\n");
		/*
		 * Switch to next frame up
		 */
		if (frame[FR_RFP] == 0)
			break; /* Top of stack */
#ifdef __PROG26
		pc = frame[FR_RLV] & R15_PC;
#else
		pc = frame[FR_RLV];
#endif

		frame = (uint32_t *)(frame[FR_RFP]);

		if (frame == NULL)
			break;

		if (INKERNEL((int)frame)) {
			/* staying in kernel */
			if (frame <= lastframe) {
				(*pr)("Bad frame pointer: %p\n", frame);
				break;
			}
		} else if (INKERNEL((int)lastframe)) {
			/* switch from user to kernel */
			if (kernel_only)
				break;	/* kernel stack only */
		} else {
			/* in user */
			if (frame <= lastframe) {
				(*pr)("Bad user frame pointer: %p\n",
					  frame);
				break;
			}
		}
		lastframe = frame;
#ifndef _KERNEL
		db_read_bytes((db_addr_t)(frame - 3), sizeof(frameb),
		    (char *)frameb);
		frame = frameb + 3;
#endif
	}
}
