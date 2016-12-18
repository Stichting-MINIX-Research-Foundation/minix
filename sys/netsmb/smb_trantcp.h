/*	$NetBSD: smb_trantcp.h,v 1.7 2014/04/25 15:52:45 pooka Exp $	*/

/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/netsmb/smb_trantcp.h,v 1.3 2002/09/18 07:38:10 bp Exp
 */
#ifndef _NETSMB_SMB_TRANTCP_H_
#define	_NETSMB_SMB_TRANTCP_H_

#ifndef _KERNEL
#error not supposed to be exposed to userland.
#endif /* !_KERNEL */

#ifdef _KERNEL

#ifdef NB_DEBUG
#define NBDEBUG(x)	aprint_debug x
#else
#define NBDEBUG(x)	/* nothing */
#endif

enum nbstate {
	NBST_CLOSED,
	NBST_RQSENT,
	NBST_SESSION,
	NBST_RETARGET,
	NBST_REFUSED
};


/*
 * socket specific data
 */
struct nbpcb {
	struct smb_vc *	nbp_vc;
	struct socket *	nbp_tso;	/* transport socket */
	struct sockaddr_nb *nbp_laddr;	/* local address */
	struct sockaddr_nb *nbp_paddr;	/* peer address */

	int		nbp_flags;
#define	NBF_LOCADDR	0x0001		/* has local addr */
#define	NBF_CONNECTED	0x0002
#define	NBF_RECVLOCK	0x0004

	enum nbstate	nbp_state;
	void *		nbp_selectid;
};

/*
 * Nominal space allocated per a NETBIOS socket.
 */
#define	NB_SNDQ		(64 * 1024)
#define	NB_RCVQ		(64 * 1024)

/*
 * Timeouts (s) used for send/receive. XXX Sysctl this?
 */
#define NB_SNDTIMEO	(5)
#define NB_RCVTIMEO	(5)

/*
 * TCP slowstart presents a problem in conjunction with large
 * reads.  To ensure a steady stream of ACKs while reading using
 * large transaction sizes, we call soreceive() with a smaller
 * buffer size.  See nbssn_recv().
 */
#define NB_SORECEIVE_CHUNK	(8 * 1024)

extern struct smb_tran_desc smb_tran_nbtcp_desc;

#endif /* _KERNEL */

#endif /* !_NETSMB_SMB_TRANTCP_H_ */
