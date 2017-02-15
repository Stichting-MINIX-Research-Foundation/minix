/*	$NetBSD: show.c,v 1.48 2015/03/23 18:33:17 roy Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)route.c	8.3 (Berkeley) 3/9/94";
#else
__RCSID("$NetBSD: show.c,v 1.48 2015/03/23 18:33:17 roy Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netmpls/mpls.h>

#include <sys/sysctl.h>

#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "keywords.h"
#include "rtutil.h"
#include "extern.h"
#include "prog_ops.h"

void
parse_show_opts(int argc, char * const *argv, int *afp, int *flagsp,
    const char **afnamep, bool nolink)
{
	const char *afname = "unspec";
	int af, flags;

	flags = 0;
	af = AF_UNSPEC;
	for (; argc >= 2; argc--) {
		if (*argv[argc - 1] != '-')
			goto bad;
		switch (keyword(argv[argc - 1] + 1)) {
		case K_HOST:
			flags |= RTF_HOST;
			break;
		case K_LLINFO:
			flags |= RTF_LLINFO;
			break;
		case K_INET:
			af = AF_INET;
			afname = argv[argc - 1] + 1;
			break;
#ifdef INET6
		case K_INET6:
			af = AF_INET6;
			afname = argv[argc - 1] + 1;
			break;
#endif
#ifndef SMALL
		case K_ATALK:
			af = AF_APPLETALK;
			afname = argv[argc - 1] + 1;
			break;
		case K_MPLS:
			af = AF_MPLS;
			afname = argv[argc - 1] + 1;
			break;
#endif /* SMALL */
		case K_LINK:
			if (nolink)
				goto bad;
			af = AF_LINK;
			afname = argv[argc - 1] + 1;
			break;
		default:
			goto bad;
		}
	}
	switch (argc) {
	case 1:
	case 0:
		break;
	default:
	bad:
		usage(argv[argc - 1]);
	}
	if (afnamep != NULL)
		*afnamep = afname;
	*afp = af;
	*flagsp = flags;
}

/*
 * Print routing tables.
 */
void
show(int argc, char *const *argv, int flags)
{
	int af, rflags;
	static int interesting = RTF_UP | RTF_GATEWAY | RTF_HOST |
	    RTF_REJECT | RTF_LLINFO | RTF_LOCAL | RTF_BROADCAST;

	parse_show_opts(argc, argv, &af, &rflags, NULL, true);
	p_rttables(af, flags, rflags, interesting);
}
