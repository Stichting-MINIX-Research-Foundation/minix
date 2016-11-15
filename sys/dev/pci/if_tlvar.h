/*	$NetBSD: if_tlvar.h,v 1.17 2015/04/13 16:33:25 riastradh Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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

/*
 * Texas Instruments ThunderLAN ethernet controller
 * ThunderLAN Programmer's Guide (TI Literature Number SPWU013A)
 * available from www.ti.com
 */

#include <sys/rndsource.h>

#include <dev/i2c/i2cvar.h>

struct tl_product_desc {
	u_int32_t tp_product;
	int tp_tlphymedia;
	const char *tp_desc;
};

struct tl_softc {
	device_t sc_dev;		/* base device */
	bus_space_tag_t tl_bustag;
	bus_space_handle_t tl_bushandle; /* CSR region handle */
	bus_dma_tag_t tl_dmatag;
	const struct tl_product_desc *tl_product;
	void* tl_ih;
	struct ethercom tl_ec;
	struct callout tl_tick_ch;	/* tick callout */
	struct callout tl_restart_ch;	/* restart callout */
	u_int8_t tl_enaddr[ETHER_ADDR_LEN];	/* hardware address */
	struct i2c_controller sc_i2c;	/* i2c controller info, for eeprom */
	mii_data_t tl_mii;		/* mii bus */
	bus_dma_segment_t ctrl_segs; /* bus-dma memory for control blocks */
	int ctrl_nsegs;
	char *ctrl;			/* vaddr for ctrl_segs */
	struct Rx_list *Rx_list;	/* Receive and transmit lists */
	struct tl_Rx_list *hw_Rx_list;	/* and assocoated hw descriptor */
	bus_dmamap_t Rx_dmamap;		/* and associated DMA maps */
	struct Tx_list *Tx_list;
	struct tl_Tx_list *hw_Tx_list;
	bus_dmamap_t Tx_dmamap;
	struct Rx_list *active_Rx, *last_Rx;
	struct Tx_list *active_Tx, *last_Tx;
	struct Tx_list *Free_Tx;
	bus_dmamap_t null_dmamap;	/* for small packets padding */
#ifdef TL_PRIV_STATS
	int ierr_overr;
	int ierr_code;
	int ierr_crc;
	int ierr_nomem;
	int oerr_underr;
	int oerr_deferred;
	int oerr_coll;
	int oerr_multicoll;
	int oerr_latecoll;
	int oerr_exesscoll;
	int oerr_carrloss;
	int oerr_mcopy;
#endif
	krndsource_t rnd_source;
};
#define tl_if            tl_ec.ec_if
#define tl_bpf   tl_if.if_bpf

typedef struct tl_softc tl_softc_t;
typedef u_long ioctl_cmd_t;

#define TL_HR_READ(sc, reg) \
	bus_space_read_4(sc->tl_bustag, sc->tl_bushandle, (reg))
#define TL_HR_READ_BYTE(sc, reg) \
	bus_space_read_1(sc->tl_bustag, sc->tl_bushandle, (reg))
#define TL_HR_WRITE(sc, reg, data) \
	bus_space_write_4(sc->tl_bustag, sc->tl_bushandle, (reg), (data))
#define TL_HR_WRITE_BYTE(sc, reg, data) \
	bus_space_write_1(sc->tl_bustag, sc->tl_bushandle, (reg), (data))
#define ETHER_MIN_TX (ETHERMIN + sizeof(struct ether_header))
