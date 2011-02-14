/* $NetBSD: compat___ntp_gettime30.c,v 1.2 2009/01/11 02:46:26 christos Exp $ */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat___ntp_gettime30.c,v 1.2 2009/01/11 02:46:26 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include <sys/time.h>
#include <sys/timex.h>
#include <compat/sys/timex.h>

__warn_references(__ntp_gettime30,
    "warning: reference to compatibility __ntp_gettime30(); include <sys/timex.h> for correct reference")

int
__ntp_gettime30(struct ntptimeval50 *ontvp)
{
	struct ntptimeval ntv;
	int res;

	res = __ntp_gettime50(&ntv);
	ontvp->time.tv_sec = (int32_t)ntv.time.tv_sec;
	ontvp->time.tv_nsec = ntv.time.tv_nsec;
	ontvp->maxerror = ntv.maxerror;
	ontvp->esterror = ntv.esterror;

	return (res);
}
