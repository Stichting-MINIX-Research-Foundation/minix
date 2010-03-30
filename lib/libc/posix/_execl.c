/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#define execl _execl
#define execve _execve
#include <unistd.h>
#include <minix/compiler.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdlib.h>
#include <alloca.h>
#include <lib.h>

#ifndef __UNCONST
#define __UNCONST(x) ((void *) (x))
#endif

extern char * const **_penviron;	/* The default environment. */

int
execl(const char *path, const char *arg, ...)
/* execl("/bin/sh", "sh", "-c", "example", (char *) 0); */
{
#if FUNC_ARGS_ARRAY
	/* 
    * Assumption:  The C-implementation for this machine pushes
	 * function arguments downwards on the stack making a perfect
	 * argument array.  Luckily this is almost always so.
	 *
	 * execl() - execute	 Author: Kees J. Bot, 21 Jan 1994
	 */
	return execve(path, (char * const *) &arg, *_penviron);
#else
   /*
    * Indeed, GCC (4.4.1) behaves similarly to ACK. Nonetheless, let's
    * stay the safe side. LLVM (llvm-gcc) doesn't make the aforementioned
	 * assumption.
	 */
	va_list ap;
	char **argv;
	int i;

	va_start(ap, arg);
	for (i = 2; va_arg(ap, char *) != NULL; i++)
	  continue;
	va_end(ap);

	if ((argv = alloca(i * sizeof (char *))) == NULL) {
		errno = ENOMEM;
		return -1;
	}
	
	va_start(ap, arg);
	argv[0] = __UNCONST(arg);
	for (i = 1; (argv[i] = va_arg(ap, char *)) != NULL; i++) 
		continue;
	va_end(ap);
	
	return execve(path, (char * const *) argv, *_penviron);
#endif
}

