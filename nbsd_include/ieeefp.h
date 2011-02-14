/*	$NetBSD: ieeefp.h,v 1.7 2005/11/18 20:02:59 christos Exp $	*/

/* 
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#ifndef _IEEEFP_H_
#define _IEEEFP_H_

#include <sys/cdefs.h>
#include <machine/ieeefp.h>

__BEGIN_DECLS
fp_rnd    fpgetround(void);
fp_rnd    fpsetround(fp_rnd);
fp_except fpgetmask(void);
fp_except fpsetmask(fp_except);
fp_except fpgetsticky(void);
fp_except fpsetsticky(fp_except);
__END_DECLS

#endif /* _IEEEFP_H_ */
