/*	$NetBSD: uuid_create.c,v 1.1 2004/09/13 21:44:54 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * Copyright (c) 2002 Hiten Mahesh Pandya
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc/uuid/uuid_create.c,v 1.2 2003/08/08 19:18:43 marcel Exp $
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: uuid_create.c,v 1.1 2004/09/13 21:44:54 thorpej Exp $");
#endif

#include "namespace.h"

#include <uuid.h>

#ifdef __minix
#include <paths.h>
#include <fcntl.h>
#include <unistd.h>

/* Fake a uuidgen() syscall */
int uuidgen(struct uuid *store, int count)
{
	int rfd;

	if((rfd = open(_PATH_RANDOM, O_RDONLY)) < 0) {
		return -1;
	}

	while(count--) {
		if(read(rfd, store++, sizeof(*store)) < sizeof(*store)) {
			close(rfd);
			return -1;
		}
	}

	close(rfd);

	return 0;
}
#endif

/*
 * uuid_create() - create an UUID.
 * See also:
 *	http://www.opengroup.org/onlinepubs/009629399/uuid_create.htm
 */
void
uuid_create(uuid_t *u, uint32_t *status)
{
	uint32_t s;

	if (uuidgen(u, 1) == -1)
		s = uuid_s_no_memory;	/* XXX */
	else
		s = uuid_s_ok;

	if (status)
		*status = s;
}
