/*	$NetBSD: at_proto.c,v 1.18 2014/05/18 14:46:16 rmind Exp $	*/

/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: at_proto.c,v 1.18 2014/05/18 14:46:16 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>

#include <sys/kernel.h>
#include <net/if.h>
#include <net/if_ether.h>
#include <netinet/in.h>
#include <net/route.h>

#include <netatalk/at.h>
#include <netatalk/ddp.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

DOMAIN_DEFINE(atalkdomain);	/* forward declare and add to link set */

const struct protosw atalksw[] = {
    {
	.pr_type = SOCK_DGRAM,
	.pr_domain = &atalkdomain,
	.pr_protocol = ATPROTO_DDP,
	.pr_flags = PR_ATOMIC|PR_ADDR,
	.pr_output = ddp_output,
	.pr_usrreqs = &ddp_usrreqs,
	.pr_init = ddp_init,
    },
};

struct domain atalkdomain = {
	.dom_family = PF_APPLETALK,
	.dom_name = "appletalk",
	.dom_init = NULL,
	.dom_externalize = NULL,
	.dom_dispose = NULL,
	.dom_protosw = atalksw,
	.dom_protoswNPROTOSW = &atalksw[__arraycount(atalksw)],
	.dom_rtattach = rt_inithead,
	.dom_rtoffset = 32,
	.dom_maxrtkey = sizeof(struct sockaddr_at),
	.dom_ifattach = NULL,
	.dom_ifdetach = NULL,
	.dom_ifqueues = { &atintrq1, &atintrq2 },
	.dom_link = { NULL },
	.dom_mowner = MOWNER_INIT("",""),
	.dom_sa_cmpofs = offsetof(struct sockaddr_at, sat_addr),
	.dom_sa_cmplen = sizeof(struct at_addr),
	.dom_rtcache = LIST_HEAD_INITIALIZER(atalkdomain.dom_rtcache)
};

int
sockaddr_at_cmp(const struct sockaddr *sa1, const struct sockaddr *sa2)
{
	int rc;
	uint_fast8_t len;
	const uint_fast8_t addrofs = offsetof(struct sockaddr_at, sat_addr),
			   addrend = addrofs + sizeof(struct at_addr);
	const struct sockaddr_at *sat1, *sat2;

	sat1 = satocsat(sa1);
	sat2 = satocsat(sa2);

	len = MIN(addrend, MIN(sat1->sat_len, sat2->sat_len));

	if (len > addrofs &&
	    (rc = memcmp(&sat1->sat_addr, &sat2->sat_addr, len - addrofs)) != 0)
		return rc;

	return sat1->sat_len - sat2->sat_len;
}
