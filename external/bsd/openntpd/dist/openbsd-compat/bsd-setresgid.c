/* $Id: bsd-setresgid.c,v 1.3 2005/07/06 06:24:52 dtucker Exp $ */

/*
 * Copyright (c) 2004, 2005 Darren Tucker (dtucker at zip com au).
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"
#if !defined(HAVE_SETRESGID) && !defined(BROKEN_SETRESGID)

#include <sys/types.h>
#include <unistd.h>

int
setresgid(gid_t rgid, gid_t egid, gid_t sgid)
{
	int ret;

	/* this is the only configuration tested */
	if (rgid != egid || egid != sgid)
		return -1;
# if defined(HAVE_SETREGID) && !defined(BROKEN_SETREGID)
	if ((ret = setregid(rgid, egid)) == -1)
		return -1;
# else
	if (setegid(egid) == -1)
		return -1;
	if((ret = setgid(rgid)) == -1)
		return -1;
# endif
	return ret;
}
#endif
