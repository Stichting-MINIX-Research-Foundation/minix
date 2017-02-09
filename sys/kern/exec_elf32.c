/*	$NetBSD: exec_elf32.c,v 1.141 2014/07/22 08:18:33 maxv Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: exec_elf32.c,v 1.141 2014/07/22 08:18:33 maxv Exp $");

#define	ELFSIZE	32

#include "exec_elf.c"

#include <sys/module.h>

#define ELF32_AUXSIZE (howmany(ELF_AUX_ENTRIES * sizeof(Aux32Info), \
    sizeof(Elf32_Addr)) + MAXPATHLEN + ALIGN(1))

#ifdef COREDUMP
#define	DEP	"coredump"
#else
#define	DEP	NULL
#endif

MODULE(MODULE_CLASS_EXEC, exec_elf32, DEP);

static struct execsw exec_elf32_execsw[] = {
	{
		.es_hdrsz = sizeof (Elf32_Ehdr),
	  	.es_makecmds = exec_elf32_makecmds,
	  	.u = {
			.elf_probe_func = netbsd_elf32_probe,
		},
		.es_emul = &emul_netbsd,
		.es_prio = EXECSW_PRIO_FIRST,
		.es_arglen = ELF32_AUXSIZE,
		.es_copyargs = elf32_copyargs,
		.es_setregs = NULL,
		.es_coredump = coredump_elf32,
		.es_setup_stack = exec_setup_stack,
	},
#if EXEC_ELF_NOTELESS
	{
		.es_hdrsz = sizeof (Elf32_Ehdr),
	  	.es_makecmds = exec_elf32_makecmds,
	  	.u {
			elf_probe_func = NULL,
		},
		.es_emul = &emul_netbsd,
		.es_prio = EXECSW_PRIO_LAST,
		.es_arglen = ELF32_AUXSIZE,
		.es_copyargs = elf32_copyargs,
		.es_setregs = NULL,
		.es_coredump = coredump_elf32,
		.es_setup_stack = exec_setup_stack,
	},
#endif
};

static int
exec_elf32_modcmd(modcmd_t cmd, void *arg)
{
#if ARCH_ELFSIZE == 64
	/*
	 * If we are on a 64bit system, we don't want the 32bit execsw[] to be
	 * added in the global array, because the exec_elf32 module only works
	 * on 32bit systems.
	 *
	 * However, we need the exec_elf32 module, because it will make the 32bit
	 * functions available for netbsd32 and linux32.
	 *
	 * Therefore, allow this module on 64bit systems, but make it dormant.
	 */

	(void)exec_elf32_execsw; /* unused */

	switch (cmd) {
	case MODULE_CMD_INIT:
	case MODULE_CMD_FINI:
		return 0;
	default:
		return ENOTTY;
	}
#else /* ARCH_ELFSIZE == 64 */
	switch (cmd) {
	case MODULE_CMD_INIT:
		return exec_add(exec_elf32_execsw,
		    __arraycount(exec_elf32_execsw));

	case MODULE_CMD_FINI:
		return exec_remove(exec_elf32_execsw,
		    __arraycount(exec_elf32_execsw));

	default:
		return ENOTTY;
        }
#endif /* ARCH_ELFSIZE == 64 */
}
