/* $NetBSD: compat_semctl.c,v 1.3 2011/01/31 22:51:39 christos Exp $ */

/*
 * Copyright (c) 1994, 1995 Christopher G. Demetriou
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_semctl.c,v 1.3 2011/01/31 22:51:39 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/null.h>
#include <compat/sys/sem.h>
#include <stdarg.h>

int
semctl(int semid, int semnum, int cmd, ...)
{
	va_list ap;
	union __semun semun;
	struct semid_ds ds;
	struct semid_ds14 *ds14;
	int error;

	va_start(ap, cmd);
	switch (cmd) {
	case IPC_SET:
	case IPC_STAT:
	case GETALL:
	case SETVAL:
	case SETALL:
#ifdef __lint__
		memcpy(&semun, &ap, sizeof(semun));
#else
		semun = va_arg(ap, union __semun);
#endif
		break;
	default:
		break;
	}
	va_end(ap);

	switch (cmd) {
	case IPC_SET:
	case IPC_STAT:
		ds14 = (void *)semun.buf;
		if (cmd == IPC_SET)
			__semid_ds14_to_native(ds14, &ds);
		semun.buf = &ds;
		break;
	default:
		ds14 = NULL;
		break;
	}

	error = __semctl50(semid, semnum, cmd, &semun);
	if (error)
		return error;

	if (cmd == IPC_STAT)
		__native_to_semid_ds14(&ds, ds14);
	return 0;
}
