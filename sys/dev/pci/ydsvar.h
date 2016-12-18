/*	$NetBSD: ydsvar.h,v 1.11 2011/11/23 23:07:36 jmcneill Exp $	*/

/*
 * Copyright (c) 2000, 2001 Kazuki Sakamoto and Minoura Makoto.
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
 */

#ifndef _DEV_PCI_YDSVAR_H_
#define	_DEV_PCI_YDSVAR_H_

#define N_PLAY_SLOTS		2		/* We use only 2 (R and L) */
#define	N_PLAY_SLOT_CTRL	2
#define WORK_SIZE		0x0400

/*
 * softc
 */
struct yds_dma {
	bus_dmamap_t map;
	void *addr;			/* VA */
	bus_dma_segment_t segs[1];
	int nsegs;
	size_t size;
	struct yds_dma *next;
};

struct yds_codec_softc {
	struct yds_softc *sc;
	int id;
	int status_data;
	int status_addr;
	struct ac97_host_if host_if;
	struct ac97_codec_if *codec_if;
};

struct yds_softc {
	device_t		sc_dev;
	kmutex_t		sc_lock;
	kmutex_t		sc_intr_lock;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_pcitag;
	pcireg_t		sc_id;
	int			sc_revision;
	void			*sc_ih;		/* interrupt vectoring */
	bus_space_tag_t		memt;
	bus_space_handle_t	memh;
	bus_dma_tag_t		sc_dmatag;	/* DMA tag */
	u_int			sc_flags;

	struct yds_codec_softc	sc_codec[2];	/* Primary/Secondary AC97 */

	struct yds_dma		*sc_dmas;	/* List of DMA handles */

	/*
	 * Play/record status
	 */
	struct {
		void		(*intr)(void *); /* rint/pint */
		void		*intr_arg;	/* arg for intr */
		u_int		offset;		/* filled up to here */
		u_int		blksize;
		u_int		factor;		/* byte per sample */
		u_int		length;		/* ring buffer length */
		struct yds_dma	*dma;		/* DMA handle for ring buf */
	} sc_play, sc_rec;

	/*
	 * DSP control data
	 *
	 * Work space, play control data table, play slot control data,
	 * rec slot control data and effect slot control data are
	 * stored in a single memory segment in this order.
	 */
	struct yds_dma			sc_ctrldata;
	/* KVA and offset in buffer of play ctrl data tbl */
	uint32_t			*ptbl;
	off_t				ptbloff;
	/* KVA and offset in buffer of rec slot ctrl data */
	struct rec_slot_ctrl_bank	*rbank;
	off_t				rbankoff;
	/* Array of KVA pointers and offset of play slot control data */
	struct play_slot_ctrl_bank	*pbankp[N_PLAY_SLOT_CTRL_BANK
					       *N_PLAY_SLOTS];
	off_t				pbankoff;

	/*
	 * Legacy support
	 */
	bus_space_tag_t		sc_legacy_iot;
	bus_space_handle_t	sc_opl_ioh;
	device_t 		sc_mpu;
	bus_space_handle_t	sc_mpu_ioh;

	struct audio_encoding_set *sc_encodings;

	/*
	 * Power management
	 */
	struct pci_conf_state	sc_pciconf;
	pcireg_t		sc_dsctrl;
	pcireg_t		sc_legacy;
	pcireg_t		sc_ba[2];
};
#define sc_opl_iot	sc_legacy_iot
#define sc_mpu_iot	sc_legacy_iot

#endif /* _DEV_PCI_YDSVAR_H_ */
