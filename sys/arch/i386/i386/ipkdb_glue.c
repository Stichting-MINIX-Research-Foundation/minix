/*	$NetBSD: ipkdb_glue.c,v 1.14 2010/04/28 19:17:03 dyoung Exp $	*/

/*
 * Copyright (C) 2000 Wolfgang Solfrank.
 * Copyright (C) 2000 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NO TLIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRUCT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ipkdb_glue.c,v 1.14 2010/04/28 19:17:03 dyoung Exp $");

#include "opt_ipkdb.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <ipkdb/ipkdb.h>

#include <machine/ipkdb.h>
#include <machine/psl.h>
#include <machine/cpufunc.h>

int ipkdbregs[NREG];

int ipkdb_trap_glue(struct trapframe);

#ifdef	IPKDB_NE_PCI
#include <dev/pci/pcivar.h>

int ne_pci_ipkdb_attach(struct ipkdb_if *, bus_space_tag_t,		/* XXX */
			pci_chipset_tag_t, int, int);
#endif

static char ipkdb_mode = IPKDB_CMD_EXIT;

void
ipkdbinit(void)
{
}

int
ipkdb_poll(void)
{
	/* For now */
	return 0;
}

void
ipkdb_trap(void)
{
	ipkdb_mode = IPKDB_CMD_STEP;
	x86_write_flags(x86_read_flags() | PSL_T);
}

int
ipkdb_trap_glue(struct trapframe frame)
{
	if (ISPL(frame.tf_cs) != SEL_KPL)
		return 0;

	if (ipkdb_mode == IPKDB_CMD_EXIT
	    || (ipkdb_mode != IPKDB_CMD_STEP && frame.tf_trapno == T_TRCTRAP))
		return 0;

	x86_disable_intr();		/* Interrupts need to be disabled while in IPKDB */
	ipkdbregs[EAX] = frame.tf_eax;
	ipkdbregs[ECX] = frame.tf_ecx;
	ipkdbregs[EDX] = frame.tf_edx;
	ipkdbregs[EBX] = frame.tf_ebx;
	ipkdbregs[ESP] = (int)&frame.tf_esp;
	ipkdbregs[EBP] = frame.tf_ebp;
	ipkdbregs[ESI] = frame.tf_esi;
	ipkdbregs[EDI] = frame.tf_edi;
	ipkdbregs[EIP] = frame.tf_eip;
	ipkdbregs[EFLAGS] = frame.tf_eflags;
	ipkdbregs[CS] = frame.tf_cs;
	ipkdbregs[SS] = 0x10;
	ipkdbregs[DS] = frame.tf_ds;
	ipkdbregs[ES] = frame.tf_es;
	ipkdbregs[FS] = frame.tf_fs;
	ipkdbregs[GS] = frame.tf_gs;

	switch ((ipkdb_mode = ipkdbcmds())) {
	case IPKDB_CMD_EXIT:
	case IPKDB_CMD_RUN:
		ipkdbregs[EFLAGS] &= ~PSL_T;
		break;
	case IPKDB_CMD_STEP:
		ipkdbregs[EFLAGS] |= PSL_T;
		break;
	}
	frame.tf_eax = ipkdbregs[EAX];
	frame.tf_ecx = ipkdbregs[ECX];
	frame.tf_edx = ipkdbregs[EDX];
	frame.tf_ebx = ipkdbregs[EBX];
	frame.tf_ebp = ipkdbregs[EBP];
	frame.tf_esi = ipkdbregs[ESI];
	frame.tf_edi = ipkdbregs[EDI];
	frame.tf_eip = ipkdbregs[EIP];
	frame.tf_eflags = ipkdbregs[EFLAGS];
	frame.tf_cs = ipkdbregs[CS];
	frame.tf_ds = ipkdbregs[DS];
	frame.tf_es = ipkdbregs[ES];
	frame.tf_fs = ipkdbregs[FS];
	frame.tf_gs = ipkdbregs[GS];

	return 1;
}

int
ipkdbif_init(struct ipkdb_if *kip)
{
#ifdef IPKDB_NE_PCI
	pci_mode_detect();	/* XXX */

#ifndef IPKDB_NE_PCISLOT
#error You must specify the IPKDB_NE_PCISLOT to use IPKDB_NE_PCI.
#endif

	if (ne_pci_ipkdb_attach(kip, x86_bus_space_io, NULL, 0,
	    IPKDB_NE_PCISLOT) == 0) {
		printf("IPKDB on %s\n", kip->name);
		return 0;
	}
#endif /* IPKDB_NE_PCI */
	return -1;
}
