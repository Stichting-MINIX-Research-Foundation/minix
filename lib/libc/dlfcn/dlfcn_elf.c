/*	$NetBSD: dlfcn_elf.c,v 1.13 2012/06/24 15:26:03 christos Exp $	*/

/*
 * Copyright (c) 2000 Takuya SHIOZAKI
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: dlfcn_elf.c,v 1.13 2012/06/24 15:26:03 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/atomic.h>
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#undef dlopen
#undef dlclose
#undef dlsym
#undef dlerror
#undef dladdr
#undef dfinfo

#define	dlopen		___dlopen
#define	dlclose		___dlclose
#define	dlsym		___dlsym
#define	dlvsym		___dlvsym
#define	dlerror		___dlerror
#define	dladdr		___dladdr
#define	dlinfo		___dlinfo
#define	dl_iterate_phdr		___dl_iterate_phdr

#define ELFSIZE ARCH_ELFSIZE
#include "rtld.h"

#ifdef __weak_alias
__weak_alias(dlopen,___dlopen)
__weak_alias(dlclose,___dlclose)
__weak_alias(dlsym,___dlsym)
__weak_alias(dlvsym,___dlvsym)
__weak_alias(dlerror,___dlerror)
__weak_alias(dladdr,___dladdr)
__weak_alias(dlinfo,___dlinfo)
__weak_alias(dl_iterate_phdr,___dl_iterate_phdr)

__weak_alias(__dlopen,___dlopen)
__weak_alias(__dlclose,___dlclose)
__weak_alias(__dlsym,___dlsym)
__weak_alias(__dlvsym,___dlvsym)
__weak_alias(__dlerror,___dlerror)
__weak_alias(__dladdr,___dladdr)
__weak_alias(__dlinfo,___dlinfo)
__weak_alias(__dl_iterate_phdr,___dl_iterate_phdr)
#endif

/*
 * For ELF, the dynamic linker directly resolves references to its
 * services to functions inside the dynamic linker itself.  These
 * weak-symbol stubs are necessary so that "ld" won't complain about
 * undefined symbols.  The stubs are executed only when the program is
 * linked statically, or when a given service isn't implemented in the
 * dynamic linker.  They must return an error if called, and they must
 * be weak symbols so that the dynamic linker can override them.
 */

static char dlfcn_error[] = "Service unavailable";

/*ARGSUSED*/
void *
dlopen(const char *name, int mode)
{

	return NULL;
}

/*ARGSUSED*/
int
dlclose(void *fd)
{

	return -1;
}

/*ARGSUSED*/
void *
dlsym(void *handle, const char *name)
{

	return NULL;
}

/*ARGSUSED*/
void *
dlvsym(void *handle, const char *name, const char *version)
{

	return NULL;
}

/*ARGSUSED*/
__aconst char *
dlerror(void)
{

	return dlfcn_error;
}

/*ARGSUSED*/
int
dladdr(const void *addr, Dl_info *dli)
{

	return 0;
}

/*ARGSUSED*/
int
dlinfo(void *handle, int req, void *v)
{

	return -1;
}

static const char *dlpi_name;
static Elf_Addr dlpi_addr;
static const Elf_Phdr *dlpi_phdr;
static Elf_Half dlpi_phnum;

static void
dl_iterate_phdr_setup(void)
{
	const AuxInfo *aux;

	_DIAGASSERT(_dlauxinfo() != NULL);

	for (aux = _dlauxinfo(); aux->a_type != AT_NULL; ++aux) {
		switch (aux->a_type) {
		case AT_BASE:
			dlpi_addr = aux->a_v;
			break;
		case AT_PHDR:
			dlpi_phdr = (void *)aux->a_v;
			break;
		case AT_PHNUM:
			_DIAGASSERT(__type_fit(Elf_Half, aux->a_v));
			dlpi_phnum = (Elf_Half)aux->a_v;
			break;
		case AT_SUN_EXECNAME:
			dlpi_name = (void *)aux->a_v;
			break;
		}
	}
}

/*ARGSUSED*/
int
dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *),
    void *data)
{
	static bool setup_done;
	struct dl_phdr_info phdr_info;

	if (!setup_done) {
		/*
		 * This can race on the first call to dl_iterate_phdr.
		 * dl_iterate_phdr_setup only touches field of pointer size
		 * and smaller and such stores are atomic.
		 */
		dl_iterate_phdr_setup();
		membar_producer();
		setup_done = true;
	}

	memset(&phdr_info, 0, sizeof(phdr_info));
	phdr_info.dlpi_addr = dlpi_addr;
	phdr_info.dlpi_phdr = dlpi_phdr;
	phdr_info.dlpi_phnum = dlpi_phnum;
	phdr_info.dlpi_name = dlpi_name;

	return callback(&phdr_info, sizeof(phdr_info), data);
}
