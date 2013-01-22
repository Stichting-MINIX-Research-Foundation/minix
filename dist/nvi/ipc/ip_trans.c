/*	$NetBSD: ip_trans.c,v 1.1.1.2 2008/05/18 14:31:25 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: ip_trans.c,v 8.18 2001/06/25 15:19:25 skimo Exp (Berkeley) Date: 2001/06/25 15:19:25";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <bitstring.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "../common/common.h"
#include "ip.h"
#include "ipc_def.h"

static char ibuf[2048];				/* Input buffer. */
static size_t ibuf_len;				/* Length of current input. */

extern IPFUNLIST const ipfuns[];

/*
 * vi_input --
 *	Read from the vi message queue.
 *
 * PUBLIC: int vi_input __P((IPVIWIN *, int));
 */
int
vi_input(IPVIWIN *ipviwin, int fd)
{
	ssize_t nr;

	/* Read waiting vi messages and translate to X calls. */
	switch (nr = read(fd, ibuf + ibuf_len, sizeof(ibuf) - ibuf_len)) {
	case 0:
		return (0);
	case -1:
		return (-1);
	default:
		break;
	}
	ibuf_len += nr;

	/* Parse to data end or partial message. */
	(void)vi_translate(ipviwin, ibuf, &ibuf_len, NULL);

	return (ibuf_len > 0);
}

/*
 * vi_wsend --
 *	Construct and send an IP buffer, and wait for an answer.
 *
 * PUBLIC: int vi_wsend __P((IPVIWIN*, char *, IP_BUF *));
 */
int
vi_wsend(IPVIWIN *ipviwin, char *fmt, IP_BUF *ipbp)
{
	fd_set rdfd;
	ssize_t nr;

	if (vi_send(ipviwin->ofd, fmt, ipbp))
		return (1);

	FD_ZERO(&rdfd);
	ipbp->code = CODE_OOB;

	for (;;) {
		FD_SET(ipviwin->ifd, &rdfd);
		if (select(ipviwin->ifd + 1, &rdfd, NULL, NULL, NULL) != 0)
			return (-1);

		/* Read waiting vi messages and translate to X calls. */
		switch (nr =
		    read(ipviwin->ifd, ibuf + ibuf_len, sizeof(ibuf) - ibuf_len)) {
		case 0:
			return (0);
		case -1:
			return (-1);
		default:
			break;
		}
		ibuf_len += nr;

		/* Parse to data end or partial message. */
		(void)vi_translate(ipviwin, ibuf, &ibuf_len, ipbp);

		if (ipbp->code != CODE_OOB)
			break;
	}
	return (0);
}

/*
 * vi_translate --
 *	Translate vi messages into function calls.
 *
 * PUBLIC: int vi_translate __P((IPVIWIN *, char *, size_t *, IP_BUF *));
 */
int
vi_translate(IPVIWIN *ipviwin, char *bp, size_t *lenp, IP_BUF *ipbp)
{
	IP_BUF ipb;
	size_t len, needlen;
	u_int32_t *vp;
	char *fmt, *p, *s_bp;
	const char **vsp;
	IPFunc fun;

	for (s_bp = bp, len = *lenp; len > 0;) {
		fmt = ipfuns[(ipb.code = bp[0])-1].format;

		p = bp + IPO_CODE_LEN;
		needlen = IPO_CODE_LEN;
		for (; *fmt != '\0'; ++fmt)
			switch (*fmt) {
			case '1':				/* Value #1. */
				vp = &ipb.val1;
				goto value;
			case '2':				/* Value #2. */
				vp = &ipb.val2;
				goto value;
			case '3':				/* Value #3. */
				vp = &ipb.val3;
value:				needlen += IPO_INT_LEN;
				if (len < needlen)
					goto partial;
				memcpy(vp, p, IPO_INT_LEN);
				*vp = ntohl(*vp);
				p += IPO_INT_LEN;
				break;
			case 'a':				/* String #1. */
				vp = &ipb.len1;
				vsp = &ipb.str1;
				goto string;
			case 'b':				/* String #2. */
				vp = &ipb.len2;
				vsp = &ipb.str2;
string:				needlen += IPO_INT_LEN;
				if (len < needlen)
					goto partial;
				memcpy(vp, p, IPO_INT_LEN);
				*vp = ntohl(*vp);
				p += IPO_INT_LEN;
				needlen += *vp;
				if (len < needlen)
					goto partial;
				*vsp = p;
				p += *vp;
				break;
			}
		bp += needlen;
		len -= needlen;

		/*
		 * XXX
		 * Protocol error!?!?
		 */
		if (ipb.code >= SI_EVENT_SUP) {
			len = 0;
			break;
		}

		/*
		 * If we're waiting for a reply and we got it, return it, and
		 * leave any unprocessed data in the buffer.  If we got a reply
		 * and we're not waiting for one, discard it -- callers wait
		 * for responses.
		 */
		if (ipb.code == SI_REPLY) {
			if (ipbp == NULL)
				continue;
			*ipbp = ipb;
			break;
		}

		/* Call the underlying routine. */
		fun = *(IPFunc *)
		    (((char *)ipviwin->si_ops)+ipfuns[ipb.code - 1].offset);
		if (fun != NULL &&
		    ipfuns[ipb.code - 1].unmarshall(ipviwin, &ipb, fun))
			break;
	}
partial:
	if ((*lenp = len) != 0)
		memmove(s_bp, bp, len);
	return (0);
}
