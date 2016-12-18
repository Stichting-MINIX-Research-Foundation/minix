/*	$NetBSD: rf_utils.c,v 1.16 2006/11/16 01:33:23 christos Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/****************************************
 *
 * rf_utils.c -- various support routines
 *
 ****************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf_utils.c,v 1.16 2006/11/16 01:33:23 christos Exp $");

#include "rf_archs.h"
#include "rf_utils.h"
#include "rf_debugMem.h"
#include "rf_alloclist.h"

/* creates & zeros 2-d array with b rows and k columns (MCH) */
RF_RowCol_t **
rf_make_2d_array(int b, int k, RF_AllocListElem_t *allocList)
{
	RF_RowCol_t **retval, i;

	RF_MallocAndAdd(retval, b * sizeof(RF_RowCol_t *), (RF_RowCol_t **), allocList);
	for (i = 0; i < b; i++) {
		RF_MallocAndAdd(retval[i], k * sizeof(RF_RowCol_t), (RF_RowCol_t *), allocList);
		(void) memset((char *) retval[i], 0, k * sizeof(RF_RowCol_t));
	}
	return (retval);
}

#if (RF_INCLUDE_PARITY_DECLUSTERING > 0) || (RF_INCLUDE_PARITY_DECLUSTERING_PQ > 0)

void
rf_free_2d_array(RF_RowCol_t **a, int b, int k)
{
	RF_RowCol_t i;

	for (i = 0; i < b; i++)
		RF_Free(a[i], k * sizeof(RF_RowCol_t));
	RF_Free(a, b * sizeof(RF_RowCol_t));
}


/* creates & zeros a 1-d array with c columns */
RF_RowCol_t *
rf_make_1d_array(int c, RF_AllocListElem_t *allocList)
{
	RF_RowCol_t *retval;

	RF_MallocAndAdd(retval, c * sizeof(RF_RowCol_t), (RF_RowCol_t *), allocList);
	(void) memset((char *) retval, 0, c * sizeof(RF_RowCol_t));
	return (retval);
}

void
rf_free_1d_array(RF_RowCol_t *a, int n)
{
	RF_Free(a, n * sizeof(RF_RowCol_t));
}

/* Euclid's algorithm:  finds and returns the greatest common divisor
 * between a and b.     (MCH)
 */
int
rf_gcd(int m, int n)
{
	int     t;

	while (m > 0) {
		t = n % m;
		n = m;
		m = t;
	}
	return (n);
}
#endif
/* these convert between text and integer.  Apparently the regular C macros
 * for doing this are not available in the kernel
 */

#define ISDIGIT(x)   ( (x) >= '0' && (x) <= '9' )
#define ISHEXCHAR(x) ( ((x) >= 'a' && (x) <= 'f') || ((x) >= 'A' && (x) <= 'F') )
#define ISHEX(x)     ( ISDIGIT(x) || ISHEXCHAR(x) )
#define HC2INT(x)    ( ((x) >= 'a' && (x) <= 'f') ? (x) - 'a' + 10 :                    \
		       ( ((x) >= 'A' && (x) <= 'F') ? (x) - 'A' + 10 : (x - '0') ) )

int
rf_atoi(char *p)
{
	int     val = 0, negate = 0;

	if (*p == '-') {
		negate = 1;
		p++;
	}
	for (; ISDIGIT(*p); p++)
		val = 10 * val + (*p - '0');
	return ((negate) ? -val : val);
}

int
rf_htoi(char *p)
{
	int     val = 0;
	for (; ISHEXCHAR(*p); p++)
		val = 16 * val + HC2INT(*p);
	return (val);
}
