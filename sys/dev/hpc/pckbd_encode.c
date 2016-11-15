/*	$NetBSD: pckbd_encode.c,v 1.4 2005/12/11 12:21:22 christos Exp $	*/

/*-
 * Copyright (c) 2000 TAKEMRUA, Shin All rights reserved.
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
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pckbd_encode.c,v 1.4 2005/12/11 12:21:22 christos Exp $");

#include "opt_wsdisplay_compat.h"

#ifdef WSDISPLAY_COMPAT_RAWKBD
#include <sys/param.h>
#include <dev/wscons/wsconsio.h>
#include <dev/pckbport/pckbdreg.h>
#include <dev/hpc/pckbd_encode.h>

/*
 * pckbd_encode() is inverse function of pckbd_decode() in dev/pckbc/pckbd.c.
 */
int
pckbd_encode(u_int type, int datain, u_char *dataout)
{
	int res;
	u_char updown;

	res = 0;
	updown = (type == WSCONS_EVENT_KEY_UP) ? 0x80 : 0;

	/* 0x7f means BREAK key */
	if (datain == 0x7f) {
		dataout[res++] = KBR_EXTENDED1;
		dataout[res++] = (0x1d | updown);
		datain = 0x45;
	}

 	/* extended keys */
	if (datain & 0x80) {
		dataout[res++] = KBR_EXTENDED0;
		datain &= 0x7f;
	}

	dataout[res++] = (datain | updown);

	return (res);
}
#endif /* WSDISPLAY_COMPAT_RAWKBD */
