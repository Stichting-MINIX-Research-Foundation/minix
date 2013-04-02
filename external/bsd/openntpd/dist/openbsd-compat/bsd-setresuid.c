/* $Id: bsd-setresuid.c,v 1.3 2005/07/06 06:24:52 dtucker Exp $ */

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
#if !defined(HAVE_SETRESUID) && !defined(BROKEN_SETRESUID)

#include <sys/types.h>
#include <unistd.h>

int
setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
	uid_t ouid;
	int ret = -1;

	/* Allow only the tested configuration. */
	if (ruid != euid || euid != suid) {
		errno = ENOSYS;
		return -1;
	}
	ouid = getuid();

# if defined(HAVE_SETREUID) && !defined(BROKEN_SETREUID)
	if ((ret = setreuid(euid, euid)) == -1)
		return -1;
# else
#  ifndef SETEUID_BREAKS_SETUID
	if (seteuid(euid) == -1)
		return -1;
#  endif
	if ((ret = setuid(ruid)) == -1)
		return -1;
# endif

	/*
	 * When real, effective and saved uids are the same and we have
	 * changed uids, sanity check that we cannot restore the old uid.
	 */
	if (ruid == euid && euid == suid && ouid != ruid &&
	    setuid(ouid) != -1 && seteuid(ouid) != -1) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Finally, check that the real and effective uids are what we
	 * expect.
	 */
	if (getuid() != ruid || geteuid() != euid) {
		errno = EACCES;
		return -1;
	}

	return ret;
}
#endif
