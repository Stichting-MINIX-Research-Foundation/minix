/*	$NetBSD: compat___semctl13.c,v 1.6 2015/01/29 20:44:38 joerg Exp $ */

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat___semctl13.c,v 1.6 2015/01/29 20:44:38 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#define __LIBC12_SOURCE__
#include <stdarg.h>
#include <sys/time.h>
#include <compat/sys/time.h>
#include <sys/sem.h>
#include <compat/sys/sem.h>
#ifdef __lint__
#include <string.h>
#endif

__warn_references(__semctl13,
    "warning: reference to compatibility __semctl13(); include <sys/sem.h> to generate correct reference")

/*
 * Copy timeout to local variable and call the syscall.
 */
int
__semctl13(int semid, int semnum, int cmd, ...)
{
	va_list ap;
	union __semun semun;
	struct semid_ds13 *ds13;
	struct semid_ds ds;
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
		ds13 = (void *)semun.buf;
		semun.buf = &ds;
		if (cmd == IPC_SET)
			__semid_ds13_to_native(ds13, &ds);
		break;
	default:
		ds13 = NULL;
		break;
	}


	error = ____semctl50(semid, semnum, cmd, &semun);
	if (error)
		return error;

	if (cmd == IPC_STAT)
		__native_to_semid_ds13(&ds, ds13);
	return 0;
}
