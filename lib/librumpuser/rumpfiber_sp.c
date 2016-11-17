/*	$NetBSD: rumpfiber_sp.c,v 1.4 2015/02/15 00:54:32 justin Exp $	*/

/*
 * Copyright (c) 2014 Justin Cormack.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* stubs for sp functions as not supported for fibers yet */

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpfiber_sp.c,v 1.4 2015/02/15 00:54:32 justin Exp $");
#endif /* !lint */

#include <stdint.h>
#include <stdlib.h>

#include <rump/rumpuser.h>

#include "rumpfiber.h"

/*ARGSUSED*/
int
rumpuser_sp_init(const char *url,
	const char *ostype, const char *osrelease, const char *machine)
{

	return 0;
}

/*ARGSUSED*/
void
rumpuser_sp_fini(void *arg)
{

}

/*ARGSUSED*/
int
rumpuser_sp_raise(void *arg, int signo)
{

	abort();
}

/*ARGSUSED*/
int
rumpuser_sp_copyin(void *arg, const void *raddr, void *laddr, size_t len)
{

	abort();
}

/*ARGSUSED*/
int
rumpuser_sp_copyinstr(void *arg, const void *raddr, void *laddr, size_t *len)
{

	abort();
}

/*ARGSUSED*/
int
rumpuser_sp_copyout(void *arg, const void *laddr, void *raddr, size_t dlen)
{

	abort();
}

/*ARGSUSED*/
int
rumpuser_sp_copyoutstr(void *arg, const void *laddr, void *raddr, size_t *dlen)
{

	abort();
}

/*ARGSUSED*/
int
rumpuser_sp_anonmmap(void *arg, size_t howmuch, void **addr)
{

	abort();
}
