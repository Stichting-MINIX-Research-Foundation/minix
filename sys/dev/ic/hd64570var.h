/*	$NetBSD: hd64570var.h,v 1.11 2012/10/27 17:18:20 chs Exp $	*/

/*
 * Copyright (c) 1999 Christian E. Hopps
 * Copyright (c) 1998 Vixie Enterprises
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
 * 3. Neither the name of Vixie Enterprises nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY VIXIE ENTERPRISES AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL VIXIE ENTERPRISES OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for Vixie Enterprises by Michael Graff
 * <explorer@flame.org>.  To learn more about Vixie Enterprises, see
 * ``http://www.vix.com''.
 */

#ifndef _DEV_IC_HD64570VAR_H_
#define _DEV_IC_HD64570VAR_H_

#define SCA_USE_FASTQ		/* use a split queue, one for fast traffic */

#define SCA_MTU		1500	/* hard coded */

#ifndef SCA_BSIZE
#define SCA_BSIZE	(SCA_MTU + 4)	/* room for HDLC as well */
#endif


struct sca_softc;
typedef struct sca_port sca_port_t;
typedef struct sca_desc sca_desc_t;

/*
 * device DMA descriptor
 */
struct sca_desc {
	u_int16_t	sd_chainp;	/* chain pointer */
	u_int16_t	sd_bufp;	/* buffer pointer (low bits) */
	u_int8_t	sd_hbufp;	/* buffer pointer (high bits) */
	u_int8_t	sd_unused0;
	u_int16_t	sd_buflen;		/* total length */
	u_int8_t	sd_stat;	/* status */
	u_int8_t	sd_unused1;
};
#define SCA_DESC_EOT            0x01
#define SCA_DESC_CRC            0x04
#define SCA_DESC_OVRN           0x08
#define SCA_DESC_RESD           0x10
#define SCA_DESC_ABORT          0x20
#define SCA_DESC_SHRTFRM        0x40
#define SCA_DESC_EOM            0x80
#define SCA_DESC_ERRORS         0x7C

/*
 * softc structure for each port
 */
struct sca_port {
	u_int msci_off;		/* offset for msci address for this port */
	u_int dmac_off;		/* offset of dmac address for this port */

	u_int sp_port;

	/*
	 * CISCO keepalive stuff
	 */
	u_int32_t	cka_lasttx;
	u_int32_t	cka_lastrx;

	/*
	 * clock values, clockrate = sysclock / tmc / 2^div;
	 */
	u_int8_t	sp_eclock;	/* enable external clock generate */
	u_int8_t	sp_rxs;		/* recv clock source */
	u_int8_t	sp_txs;		/* transmit clock source */
	u_int8_t	sp_tmc;		/* clock constant */

	/*
	 * start of each important bit of information for transmit and
	 * receive buffers.
	 *
	 * note: for non-DMA the phys and virtual version should be
	 * the same value and should be an _offset_ from the beginning
	 * of mapped memory described by sc_memt/sc_memh.
	 */
	u_int sp_ntxdesc;		/* number of tx descriptors */
	bus_addr_t sp_txdesc_p;		/* paddress of first tx desc */
	sca_desc_t *sp_txdesc;		/* vaddress of first tx desc */
	bus_addr_t sp_txbuf_p;		/* paddress of first tx buffer */
	u_int8_t *sp_txbuf;		/* vaddress of first tx buffer */

	volatile u_int sp_txcur;	/* last descriptor in chain */
	volatile u_int sp_txinuse;	/* descriptors in use */
	volatile u_int sp_txstart;	/* start descriptor */

	u_int sp_nrxdesc;		/* number of rx descriptors */
	bus_addr_t sp_rxdesc_p;		/* paddress of first rx desc */
	sca_desc_t *sp_rxdesc;		/* vaddress of first rx desc */
	bus_addr_t sp_rxbuf_p;		/* paddress of first rx buffer */
	u_int8_t *sp_rxbuf;		/* vaddress of first rx buffer */

	u_int sp_rxstart;		/* index of first descriptor */
	u_int sp_rxend;			/* index of last descriptor */

	struct ifnet sp_if;		/* the network information */
	struct ifqueue linkq;		/* link-level packets are high prio */
#ifdef SCA_USE_FASTQ
	struct ifqueue fastq;		/* interactive packets */
#endif

	struct sca_softc *sca;		/* pointer to parent */
};

/*
 * softc structure for the chip itself
 */
struct sca_softc {
	device_t	sc_parent;	/* our parent device, or NULL */
	int		sc_numports;	/* number of ports present */
	u_int32_t	sc_baseclock;	/* the base operating clock */

	/*
	 * a callback into the parent, since the SCA chip has no control
	 * over DTR, we have to make a callback into the parent, which
	 * might know about DTR.
	 *
	 * If the function pointer is NULL, no callback is specified.
	 */
	void *sc_aux;
	void (*sc_dtr_callback)(void *, int, int);
	void (*sc_clock_callback)(void *, int, int);

	/* used to read and write the device registers */
	u_int8_t	(*sc_read_1)(struct sca_softc *, u_int);
	u_int16_t	(*sc_read_2)(struct sca_softc *, u_int);
	void		(*sc_write_1)(struct sca_softc *, u_int, u_int8_t);
	void		(*sc_write_2)(struct sca_softc *, u_int, u_int16_t);

	sca_port_t		sc_ports[2];

	bus_space_tag_t		sc_iot;		/* io space for registers */
	bus_space_handle_t	sc_ioh;		/* io space for registers */

	int			sc_usedma;
	union {
		struct {
			bus_space_tag_t	p_memt;		/* mem for non-DMA */
			bus_space_handle_t p_memh;	/* mem for non-DMA */
			bus_space_handle_t p_sca_ioh[16]; /* io for sca regs */
			bus_size_t 	p_pagesize;	/* memory page size */
			bus_size_t 	p_pagemask;	/* memory page mask */
			u_int 		p_pageshift;	/* memory page shift */
			bus_size_t 	p_npages;	/* num mem pages */

			void	(*p_set_page)(struct sca_softc *, bus_addr_t);
			void	(*p_page_on)(struct sca_softc *);
			void	(*p_page_off)(struct sca_softc *);
		} u_paged;
		struct {
			bus_dma_tag_t	d_dmat;	/* bus DMA tag */
			bus_dmamap_t	d_dmam;	/* bus DMA map */
			bus_dma_segment_t d_seg;	/* bus DMA segment */
			void *		d_dma_addr;	/* kva  of segment */
			bus_size_t	d_allocsize;	/* size of region */
		} u_dma;
	} sc_u;
};
#define	scu_memt	sc_u.u_paged.p_memt
#define	scu_memh	sc_u.u_paged.p_memh
#define	scu_sca_ioh	sc_u.u_paged.p_sca_ioh
#define	scu_pagesize	sc_u.u_paged.p_pagesize
#define	scu_pagemask	sc_u.u_paged.p_pagemask
#define	scu_pageshift	sc_u.u_paged.p_pageshift
#define	scu_npages	sc_u.u_paged.p_npages
#define	scu_set_page	sc_u.u_paged.p_set_page
#define	scu_page_on	sc_u.u_paged.p_page_on
#define	scu_page_off	sc_u.u_paged.p_page_off
#define	scu_dmat	sc_u.u_dma.d_dmat
#define	scu_dmam	sc_u.u_dma.d_dmam
#define	scu_seg		sc_u.u_dma.d_seg
#define	scu_dma_addr	sc_u.u_dma.d_dma_addr
#define	scu_allocsize	sc_u.u_dma.d_allocsize

void	sca_init(struct sca_softc *);
void	sca_port_attach(struct sca_softc *, u_int);
int	sca_hardintr(struct sca_softc *);
void	sca_shutdown(struct sca_softc *);
void	sca_get_base_clock(struct sca_softc *);
void	sca_print_clock_info(struct sca_softc *);

#endif /* _DEV_IC_HD64570VAR_H_ */
