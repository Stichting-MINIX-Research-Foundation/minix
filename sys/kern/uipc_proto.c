/*	$NetBSD: uipc_proto.c,v 1.23 2014/05/18 14:46:15 rmind Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
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
 *
 *	@(#)uipc_proto.c	8.2 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_proto.c,v 1.23 2014/05/18 14:46:15 rmind Exp $");

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/un.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/raw_cb.h>

/*
 * Definitions of protocols supported in the UNIX domain.
 */

DOMAIN_DEFINE(unixdomain);	/* forward define and add to link set */

const struct protosw unixsw[] = {
	{
		.pr_type = SOCK_STREAM,
		.pr_domain = &unixdomain,
		.pr_flags = PR_CONNREQUIRED|PR_WANTRCVD|PR_RIGHTS|PR_LISTEN,
		.pr_ctloutput = uipc_ctloutput,
		.pr_usrreqs = &unp_usrreqs,
	},
	{
		.pr_type = SOCK_DGRAM,
		.pr_domain = &unixdomain,
		.pr_flags = PR_ATOMIC|PR_ADDR|PR_RIGHTS,
		.pr_ctloutput = uipc_ctloutput,
		.pr_usrreqs = &unp_usrreqs,
	},
	{
		.pr_type = SOCK_SEQPACKET,
		.pr_domain = &unixdomain,
		.pr_flags = PR_CONNREQUIRED|PR_WANTRCVD|PR_RIGHTS|PR_LISTEN|
			    PR_ATOMIC,
		.pr_ctloutput = uipc_ctloutput,
		.pr_usrreqs = &unp_usrreqs,
	}
};

struct domain unixdomain = {
	.dom_family = AF_LOCAL,
	.dom_init = uipc_init,
	.dom_name = "unix",
	.dom_externalize = unp_externalize,
	.dom_dispose = unp_dispose,
	.dom_protosw = unixsw,
	.dom_protoswNPROTOSW = &unixsw[__arraycount(unixsw)],
};
