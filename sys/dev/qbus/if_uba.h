/*	$NetBSD: if_uba.h,v 1.15 2007/03/04 06:02:29 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)if_uba.h	7.4 (Berkeley) 6/28/90
 */

/*
 * Structure and routine definitions
 * for UNIBUS network interfaces.
 */

/*
 * Each interface has structures giving information
 * about UNIBUS resources held by the interface
 * for each send and receive buffer.
 *
 * We hold IF_NUBAMR map registers for datagram data, starting
 * at ifr_mr.  Map register ifr_mr[-1] maps the local network header
 * ending on the page boundary.  Bdp's are reserved for read and for
 * write, given by ifr_bdp.  The prototype of the map register for
 * read and for write is saved in ifr_proto.
 *
 * When write transfers are not full pages on page boundaries we just
 * copy the data into the pages mapped on the UNIBUS and start the
 * transfer.  If a write transfer is of a (1024 byte) page on a page
 * boundary, we swap in UNIBUS pte's to reference the pages, and then
 * remap the initial pages (from ifu_wmap) when the transfer completes.
 *
 * When read transfers give whole pages of data to be input, we
 * allocate page frames from a network page list and trade them
 * with the pages already containing the data, mapping the allocated
 * pages to replace the input pages for the next UNIBUS data input.
 */

/*
 * Information per interface.
 */
struct	ifubinfo {
	struct	uba_softc *iff_softc;		/* uba */
};

/*
 * Information per buffer.
 */
struct ifrw {
	short	ifrw_bdp;			/* unibus bdp */
	short	ifrw_flags;			/* type, etc. */
#define	IFRW_MBUF	0x01			/* uses DMA from mbuf */
	bus_dmamap_t ifrw_map;			/* DMA map */
	struct	mbuf *ifrw_mbuf;
};

/*
 * Information per transmit buffer, including the above.
 */
struct ifxmt {
	struct	ifrw ifrw;
	void *	ifw_vaddr;			/* DMA memory virtual addr */
	int	ifw_size;			/* Size of this DMA block */
};
#define	ifrw_addr	ifrw_mbuf->m_data
#define ifrw_info	ifrw_map->dm_segs[0].ds_addr
#define	ifw_addr	ifrw.ifrw_addr
#define	ifw_bdp		ifrw.ifrw_bdp
#define	ifw_flags	ifrw.ifrw_flags
#define	ifw_info	ifrw.ifrw_info
#define	ifw_proto	ifrw.ifrw_proto
#define	ifw_mr		ifrw.ifrw_mr
#define	ifw_map		ifrw.ifrw_map
#define ifw_mbuf	ifrw.ifrw_mbuf

/*
 * Most interfaces have a single receive and a single transmit buffer,
 * and use struct ifuba to store all of the unibus information.
 */
struct ifuba {
	struct	ifubinfo ifu_info;
	struct	ifrw ifu_r;
	struct	ifxmt ifu_xmt;
};

#define	ifu_softc	ifu_info.iff_softc
#define	ifu_hlen	ifu_info.iff_hlen
#define	ifu_uba		ifu_info.iff_uba
#define	ifu_ubamr	ifu_info.iff_ubamr
#define	ifu_flags	ifu_info.iff_flags
#define	ifu_w		ifu_xmt.ifrw
#define	ifu_xtofree	ifu_xmt.ifw_xtofree

#ifdef	_KERNEL
#define	if_ubainit(ifuba, uban, size) \
		if_ubaminit(&(ifuba)->ifu_info, uban, size, \
			&(ifuba)->ifu_r, 1, &(ifuba)->ifu_xmt, 1)
#define	if_rubaget(ifu, ifp, len) \
		if_ubaget(&(ifu)->ifu_info, &(ifu)->ifu_r, ifp, len)
#define	if_wubaput(ifu, m) \
		if_ubaput(&(ifu)->ifu_info, &(ifu)->ifu_xmt, m)
#define if_wubaend(ifu) \
		if_ubaend(&(ifu)->ifu_info, &(ifu)->ifu_xmt)

/* Prototypes */
int if_ubaminit(struct ifubinfo *, struct uba_softc *, int,
	    struct ifrw *, int, struct ifxmt *, int);
int if_ubaput(struct ifubinfo *, struct ifxmt *, struct mbuf *);
struct mbuf *if_ubaget(struct ifubinfo *, struct ifrw *, struct ifnet *, int);
void if_ubaend(struct ifubinfo *ifu, struct ifxmt *);
#endif
