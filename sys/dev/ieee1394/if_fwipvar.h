/*	$NetBSD: if_fwipvar.h,v 1.5 2010/03/29 03:05:28 kiyohara Exp $	*/
/*-
 * Copyright (c) 2004
 *	Doug Rabson
 * Copyright (c) 2002-2003
 * 	Hidetoshi Shimokawa. All rights reserved.
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
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
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
 * $FreeBSD: src/sys/dev/firewire/if_fwipvar.h,v 1.5 2007/06/06 14:31:36 simokawa Exp $
 */

#ifndef _IF_FWIPVAR_H_
#define _IF_FWIPVAR_H_

#define MTAG_FIREWIRE_HWADDR		0
#define MTAG_FIREWIRE_SENDER_EUID	1


struct fwip_softc {
	/* XXX this must be first for fd.post_explore() */
	struct firewire_dev_comm sc_fd;
	short sc_dma_ch;
	struct fw_bind sc_fwb;
	struct fw_eui64 sc_last_dest;
	struct fw_pkt sc_last_hdr;
	STAILQ_HEAD(, fw_xfer) sc_xferlist;
	struct crom_chunk sc_unit4;	/* unit directory for IPv4 */
	struct crom_chunk sc_spec4;	/* specifier description IPv4 */
	struct crom_chunk sc_ver4;	/* version description IPv4 */
	struct crom_chunk sc_unit6;	/* unit directory for IPv6 */
	struct crom_chunk sc_spec6;	/* specifier description IPv6 */
	struct crom_chunk sc_ver6;	/* version description IPv6 */
	struct fwip_eth_softc {
		struct ieee1394com fwcom;
		struct ifnet *fwip_ifp;
		struct fwip_softc *fwip;
	} sc_eth;
	kmutex_t sc_mtx;
};

#endif /* !_IF_FWIPVAR_H_ */
