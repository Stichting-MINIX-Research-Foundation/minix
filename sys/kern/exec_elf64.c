/*	$NetBSD: exec_elf64.c,v 1.6 2014/07/22 08:18:33 maxv Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: exec_elf64.c,v 1.6 2014/07/22 08:18:33 maxv Exp $");

#define	ELFSIZE	64

#include "exec_elf.c"

#include <sys/module.h>

#define ELF64_AUXSIZE (howmany(ELF_AUX_ENTRIES * sizeof(Aux64Info), \
    sizeof(Elf64_Addr)) + MAXPATHLEN + ALIGN(1))

#ifdef COREDUMP
#define	DEP	"coredump"
#else
#define	DEP	NULL
#endif

MODULE(MODULE_CLASS_EXEC, exec_elf64, DEP);

static struct execsw exec_elf64_execsw[] = {
	/* Native Elf64 */
	{
		.es_hdrsz = sizeof (Elf64_Ehdr),
	  	.es_makecmds = exec_elf64_makecmds,
	  	.u = {
			.elf_probe_func = netbsd_elf64_probe,
		},
		.es_emul = &emul_netbsd,
		.es_prio = EXECSW_PRIO_FIRST,
		.es_arglen = ELF64_AUXSIZE,
		.es_copyargs = elf64_copyargs,
		.es_setregs = NULL,
		.es_coredump = coredump_elf64,
		.es_setup_stack = exec_setup_stack,
	},
#if EXEC_ELF_NOTELESS
	/* Generic Elf64 -- run at NetBSD Elf64 */
	{
		.es_hdrsz = sizeof (Elf64_Ehdr),
		.es_makecmds = exec_elf64_makecmds,
		.u = {
			.elf_probe_func = NULL,
		},
		.es_emul = &emul_netbsd,
		.es_prio = EXECSW_PRIO_ANY,
		.es_arglen = ELF64_AUXSIZE,
		.es_copyargs = elf64_copyargs,
		.es_setregs = NULL,
		.es_coredump = coredump_elf64,
		.es_setup_stack = exec_setup_stack,
	},
#endif
};

static int
exec_elf64_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		return exec_add(exec_elf64_execsw,
		    __arraycount(exec_elf64_execsw));

	case MODULE_CMD_FINI:
		return exec_remove(exec_elf64_execsw,
		    __arraycount(exec_elf64_execsw));

	default:
		return ENOTTY;
        }
}
