/* $Id: port-qnx.c,v 1.1 2004/12/19 23:41:28 dtucker Exp $ */

/*
 * Copyright (c) 2004 Anthony O.Zabelin <rz1a at mail.ru>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(__QNX__) && !defined(HAVE_ADJTIME)

/*
 * Ajustment rate as a fraction of tick size.  Speeds or slows clock by
 * 1/rate per clock tick.
 */
#define ADJUST_RATE	128

#include <stdlib.h>
#include <sys/time.h>

int
adjtime(const struct timeval *delta, struct timeval *olddelta)
{
	long c, d, usec;
	div_t sec_usec;

	if (olddelta != NULL) {
		if (qnx_adj_time(0, 0, &c, &d) == 0) {
			sec_usec = div(((c / 1000L) * d), 1000000L);
			olddelta->tv_sec = sec_usec.quot;
			olddelta->tv_usec = sec_usec.rem;
		} else {
			olddelta->tv_sec = 0;
			olddelta->tv_usec = 0;
		}
	}
	usec = delta->tv_sec * 1000000L + delta->tv_usec;
	return(qnx_adj_time(usec, ADJUST_RATE, NULL, NULL));
}
#endif
