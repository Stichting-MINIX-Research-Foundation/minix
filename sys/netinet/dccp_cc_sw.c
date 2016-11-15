/*	$KAME: dccp_cc_sw.c,v 1.9 2005/10/21 05:33:51 nishida Exp $	*/
/*	$NetBSD: dccp_cc_sw.c,v 1.2 2015/08/24 22:21:26 pooka Exp $ */

/*
 * Copyright (c) 2003  Nils-Erik Mattsson 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Id: dccp_cc_sw.c,v 1.10 2003/05/14 08:14:46 nilmat-8 Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dccp_cc_sw.c,v 1.2 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_dccp.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>

#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>

#include <netinet/dccp.h>
#include <netinet/dccp_var.h>
#include <netinet/dccp_tcplike.h>
#include <netinet/dccp_tfrc.h>
#include <netinet/dccp_cc_sw.h>

struct dccp_cc_sw cc_sw[] = {
{ 0,			0,			0,
  0,						0,
  0,			0,			0
},
{ dccp_nocc_init,	dccp_nocc_free,		dccp_nocc_send_packet,
  dccp_nocc_send_packet_sent,			dccp_nocc_packet_recv,
  dccp_nocc_init,	dccp_nocc_free,		dccp_nocc_packet_recv
},
{ 0,			0,			0,
  0,						0,
  0,			0,			0
},
{ tcplike_send_init,	tcplike_send_free,	tcplike_send_packet,
  tcplike_send_packet_sent,			 tcplike_send_packet_recv,
   tcplike_recv_init,	tcplike_recv_free,	tcplike_recv_packet_recv
},
{ tfrc_send_init,	tfrc_send_free,		tfrc_send_packet,
  tfrc_send_packet_sent,			 tfrc_send_packet_recv,
   tfrc_recv_init,	tfrc_recv_free,		tfrc_recv_packet_recv
},
};
