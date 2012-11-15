/* $NetBSD: common.h,v 1.16 2012/03/21 10:09:20 matt Exp $ */

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * 
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

#include <sys/types.h>
#include <sys/exec.h>
#ifndef __minix
#include <sys/syscall.h>
#endif

#include <stdlib.h>
#ifdef DYNAMIC
#ifdef __weak_alias
#define dlopen	_dlopen
#define dlclose	_dlclose
#define dlsym	_dlsym
#define dlerror	_dlerror
#define dladdr	_dladdr
#endif
#include <dlfcn.h>
#include "rtld.h"
#else
typedef void Obj_Entry;
#endif

extern quad_t	__syscall(quad_t, ...);
#define	_exit(v)	__syscall(SYS_exit, (v))
#define	write(fd, s, n)	__syscall(SYS_write, (fd), (s), (n))

#define	_FATAL(str)				\
do {						\
	write(2, str, sizeof(str)-1);		\
	_exit(1);				\
} while (0)

static char	*_strrchr(char *, int);

char	**environ;
char	*__progname = __UNCONST("");
struct ps_strings *__ps_strings = 0;

extern void	_init(void);
extern void	_fini(void);
extern void	_libc_init(void);

#ifdef DYNAMIC
void	_rtld_setup(void (*)(void), const Obj_Entry *obj);

/*
 * Arrange for _DYNAMIC to be weak and undefined (and therefore to show up
 * as being at address zero, unless something else defines it).  That way,
 * if we happen to be compiling without -static but with without any
 * shared libs present, things will still work.
 */
__weakref_visible int rtld_DYNAMIC __weak_reference(_DYNAMIC);
#endif /* DYNAMIC */

#ifdef MCRT0
extern void	monstartup(u_long, u_long);
extern void	_mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */

int main(int, char **, char **);
