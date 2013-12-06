/*	$NetBSD: timex.h,v 1.2 2009/01/11 02:45:50 christos Exp $	*/

/*-
 ***********************************************************************
 *								       *
 * Copyright (c) David L. Mills 1993-2001			       *
 *								       *
 * Permission to use, copy, modify, and distribute this software and   *
 * its documentation for any purpose and without fee is hereby	       *
 * granted, provided that the above copyright notice appears in all    *
 * copies and that both the copyright notice and this permission       *
 * notice appear in supporting documentation, and that the name        *
 * University of Delaware not be used in advertising or publicity      *
 * pertaining to distribution of the software without specific,	       *
 * written prior permission. The University of Delaware makes no       *
 * representations about the suitability this software for any	       *
 * purpose. It is provided "as is" without express or implied	       *
 * warranty.							       *
 *								       *
 **********************************************************************/
#ifndef _COMPAT_SYS_TIMEX_H_
#define _COMPAT_SYS_TIMEX_H_ 1

#include <compat/sys/time.h>
/*
 * NTP user interface (ntp_gettime()) - used to read kernel clock values
 *
 * Note: The time member is in microseconds if STA_NANO is zero and
 * nanoseconds if not.
 */
struct ntptimeval50 {
	struct timespec50 time;	/* current time (ns) (ro) */
	long maxerror;		/* maximum error (us) (ro) */
	long esterror;		/* estimated error (us) (ro) */
	long tai;		/* TAI offset */
	int time_state;		/* time status */
};

struct ntptimeval30 {
	struct timeval50 time;	/* current time (ro) */
	long maxerror;		/* maximum error (us) (ro) */
	long esterror;		/* estimated error (us) (ro) */
};

#ifndef _KERNEL
#include <sys/cdefs.h>
__BEGIN_DECLS
int	ntp_gettime(struct ntptimeval30 *);
int	__ntp_gettime30(struct ntptimeval50 *);
int	__ntp_gettime50(struct ntptimeval *);
__END_DECLS
#endif /* !_KERNEL */

#endif /* _COMPAT_SYS_TIMEX_H_ */
