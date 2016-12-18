/*	$NetBSD: if_kuereg.h,v 1.19 2015/04/14 20:32:36 riastradh Exp $	*/
/*
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/usb/if_kuereg.h,v 1.2 2000/01/06 07:39:07 wpaul Exp $
 */

/*
 * Definitions for the KLSI KL5KUSB101B USB to ethernet controller.
 * The KLSI part is controlled via vendor control requests, the structure
 * of which depend a bit on the firmware running on the internal
 * microcontroller. The one exception is the 'send scan data' command,
 * which is used to load the firmware.
 */

#include <sys/rndsource.h>

#define KUE_CONFIG_NO		1
#define KUE_IFACE_IDX		0

#define KUE_CMD_GET_ETHER_DESCRIPTOR		0x00
#define KUE_CMD_SET_MCAST_FILTERS		0x01
#define KUE_CMD_SET_PKT_FILTER			0x02
#define KUE_CMD_GET_ETHERSTATS			0x03
#define KUE_CMD_GET_GPIO			0x04
#define KUE_CMD_SET_GPIO			0x05
#define KUE_CMD_SET_MAC				0x06
#define KUE_CMD_GET_MAC				0x07
#define KUE_CMD_SET_URB_SIZE			0x08
#define KUE_CMD_SET_SOFS			0x09
#define KUE_CMD_SET_EVEN_PKTS			0x0A
#define KUE_CMD_SEND_SCAN			0xFF

struct kue_ether_desc {
	uint8_t			kue_len;
	uint8_t			kue_rsvd0;
	uint8_t			kue_rsvd1;
	uint8_t			kue_macaddr[ETHER_ADDR_LEN];
	uint8_t			kue_etherstats[4];
	uint8_t			kue_maxseg[2];
	uint8_t			kue_mcastfilt[2];
	uint8_t			kue_rsvd2;
};

#define KUE_ETHERSTATS(x)	\
	le32dec((x)->kue_desc.kue_etherstats)
#define KUE_MAXSEG(x)		\
	le16dec((x)->kue_desc.kue_maxseg)
#define KUE_MCFILTCNT(x)	\
	(le16dec((x)->kue_desc.kue_mcastfilt) & 0x7FFF)
#define KUE_MCFILT(x, y)	\
	(uint8_t *)&(sc->kue_mcfilters[y * ETHER_ADDR_LEN])

#define KUE_STAT_TX_OK			0x00000001
#define KUE_STAT_RX_OK			0x00000002
#define KUE_STAT_TX_ERR			0x00000004
#define KUE_STAT_RX_ERR			0x00000008
#define KUE_STAT_RX_NOBUF		0x00000010
#define KUE_STAT_TX_UCAST_BYTES		0x00000020
#define KUE_STAT_TX_UCAST_FRAMES	0x00000040
#define KUE_STAT_TX_MCAST_BYTES		0x00000080
#define KUE_STAT_TX_MCAST_FRAMES	0x00000100
#define KUE_STAT_TX_BCAST_BYTES		0x00000200
#define KUE_STAT_TX_BCAST_FRAMES	0x00000400
#define KUE_STAT_RX_UCAST_BYTES		0x00000800
#define KUE_STAT_RX_UCAST_FRAMES	0x00001000
#define KUE_STAT_RX_MCAST_BYTES		0x00002000
#define KUE_STAT_RX_MCAST_FRAMES	0x00004000
#define KUE_STAT_RX_BCAST_BYTES		0x00008000
#define KUE_STAT_RX_BCAST_FRAMES	0x00010000
#define KUE_STAT_RX_CRCERR		0x00020000
#define KUE_STAT_TX_QUEUE_LENGTH	0x00040000
#define KUE_STAT_RX_ALIGNERR		0x00080000
#define KUE_STAT_TX_SINGLECOLL		0x00100000
#define KUE_STAT_TX_MULTICOLL		0x00200000
#define KUE_STAT_TX_DEFERRED		0x00400000
#define KUE_STAT_TX_MAXCOLLS		0x00800000
#define KUE_STAT_RX_OVERRUN		0x01000000
#define KUE_STAT_TX_UNDERRUN		0x02000000
#define KUE_STAT_TX_SQE_ERR		0x04000000
#define KUE_STAT_TX_CARRLOSS		0x08000000
#define KUE_STAT_RX_LATECOLL		0x10000000

#define KUE_RXFILT_PROMISC		0x0001
#define KUE_RXFILT_ALLMULTI		0x0002
#define KUE_RXFILT_UNICAST		0x0004
#define KUE_RXFILT_BROADCAST		0x0008
#define KUE_RXFILT_MULTICAST		0x0010

#define KUE_TIMEOUT		1000
#define ETHER_ALIGN		2
#define KUE_BUFSZ		1536
#define KUE_MIN_FRAMELEN	60

#define KUE_RX_LIST_CNT		1
#define KUE_TX_LIST_CNT		1

#define KUE_CTL_READ		0x01
#define KUE_CTL_WRITE		0x02

#define KUE_WARM_REV		0x0202

/*
 * The interrupt endpoint is currently unused
 * by the KLSI part.
 */
#define KUE_ENDPT_RX		0x0
#define KUE_ENDPT_TX		0x1
#define KUE_ENDPT_INTR		0x2
#define KUE_ENDPT_MAX		0x3

struct kue_type {
	uint16_t		kue_vid;
	uint16_t		kue_did;
};

struct kue_softc;

struct kue_chain {
	struct kue_softc	*kue_sc;
	usbd_xfer_handle	kue_xfer;
	uint8_t			*kue_buf;
	int			kue_idx;
};

struct kue_cdata {
	struct kue_chain	kue_tx_chain[KUE_TX_LIST_CNT];
	struct kue_chain	kue_rx_chain[KUE_RX_LIST_CNT];
	int			kue_tx_prod;
	int			kue_tx_cons;
	int			kue_tx_cnt;
	int			kue_rx_prod;
};

struct kue_softc {
	device_t kue_dev;

	struct ethercom		kue_ec;
	krndsource_t	rnd_source;
#define GET_IFP(sc) (&(sc)->kue_ec.ec_if)

	usbd_device_handle	kue_udev;
	usbd_interface_handle	kue_iface;
	uint16_t		kue_vendor;
	uint16_t		kue_product;
	struct kue_ether_desc	kue_desc;
	int			kue_ed[KUE_ENDPT_MAX];
	usbd_pipe_handle	kue_ep[KUE_ENDPT_MAX];
	int			kue_if_flags;
	uint16_t		kue_rxfilt;
	uint8_t			*kue_mcfilters;
	struct kue_cdata	kue_cdata;

	bool			kue_dying;
	bool			kue_attached;
	u_int			kue_rx_errs;
	struct timeval		kue_rx_notice;
};
