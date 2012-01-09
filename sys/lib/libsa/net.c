/*	$NetBSD: net.c,v 1.35 2009/04/11 10:57:55 lukem Exp $	*/

/*
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Header: net.c,v 1.9 93/08/06 19:32:15 leres Exp  (LBL)
 */

#include <sys/param.h>
#include <sys/socket.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include <net/if.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#ifdef _STANDALONE
#include "stand.h"
#define delay()
#else
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#define panic printf
#define delay() usleep(100000)
#define getsecs() time(NULL)
#endif

#include "net.h"

/*
 * Send a packet and wait for a reply, with exponential backoff.
 *
 * The send routine must return the actual number of bytes written,
 * or -1 on error.
 *
 * The receive routine can indicate success by returning the number of
 * bytes read; it can return 0 to indicate EOF; it can return -1 with a
 * non-zero errno to indicate failure; finally, it can return -1 with a
 * zero errno to indicate it isn't done yet.
 */
ssize_t
sendrecv(struct iodesc *d,
	ssize_t (*sproc)(struct iodesc *, void *, size_t),
	void *sbuf, size_t ssize,
	ssize_t (*rproc)(struct iodesc *, void *, size_t, saseconds_t),
	void *rbuf, size_t rsize)
{
	ssize_t cc;
	satime_t t, tlast;
	saseconds_t tmo, tleft;

#ifdef NET_DEBUG
	if (debug)
		printf("sendrecv: called\n");
#endif

	tmo = MINTMO;
	tlast = 0;
	tleft = 0;
	t = getsecs();
	for (;;) {
		if (tleft <= 0) {
			if (tmo >= MAXTMO) {
				errno = ETIMEDOUT;
				return -1;
			}
			cc = (*sproc)(d, sbuf, ssize);

			tleft = tmo;
			tmo <<= 1;
			if (tmo > MAXTMO)
				tmo = MAXTMO;

			if (cc == -1) {
				/* Error on transmit; wait before retrying */
				while ((getsecs() - t) < tmo)
					delay();
				tleft = 0;
				continue;
			}
			if ((size_t)cc < ssize)
				panic("sendrecv: short write! (%zd < %zu)",
				    cc, ssize);

			tlast = t;
		}

		/* Try to get a packet and process it. */
		cc = (*rproc)(d, rbuf, rsize, tleft);
		/* Return on data, EOF or real error. */
		if (cc != -1 || errno != 0)
			return cc;

		/* Timed out or didn't get the packet we're waiting for */
		t = getsecs();
		tleft -= t - tlast;
		tlast = t;
	}
}
