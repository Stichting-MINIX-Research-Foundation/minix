/* $NetBSD: compat_ntp_gettime.c,v 1.2 2009/01/11 02:46:26 christos Exp $ */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_ntp_gettime.c,v 1.2 2009/01/11 02:46:26 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include <sys/time.h>
#include <sys/timex.h>
#include <compat/sys/timex.h>

__warn_references(ntp_gettime,
    "warning: reference to compatibility ntp_gettime(); include <sys/timex.h> for correct reference")

int
ntp_gettime(struct ntptimeval30 *ontvp)
{
	struct ntptimeval ntv;
	int res;

	res = __ntp_gettime50(&ntv);
	ontvp->time.tv_sec = (int32_t)ntv.time.tv_sec;
	ontvp->time.tv_usec = ntv.time.tv_nsec / 1000;
	ontvp->maxerror = ntv.maxerror;
	ontvp->esterror = ntv.esterror;

	return (res);
}
