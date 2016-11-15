/*	$NetBSD: lan9118var.h,v 1.5 2015/04/13 16:33:24 riastradh Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LAN9118VAR_H_
#define _LAN9118VAR_H_

#include <sys/rndsource.h>

#define LAN9118_DEFAULT_TX_FIF_SZ	5 /*kB*/

#ifndef LAN9118_TX_FIF_SZ
#define LAN9118_TX_FIF_SZ	LAN9118_DEFAULT_TX_FIF_SZ
#endif
#define LAN9118_TX_DATA_FIFO_SIZE	(LAN9118_TX_FIF_SZ * 1024 - 512)
/*
 * LAN9118 has FIFO buffer, total size is 16kB.  That is using TX and RX
 * buffers.  We can set TX FIFO Size.  Also TX Status FIFO size is fixed
 * at 512 Bytes.
 *
 *  TX FIFO Size		: LAN9118_TX_FIF_SZ
 *   TX Status FIFO Size	: 512 Bytes (fixed)
 *   TX Data FIFO Size		: LAN9118_TX_FIF_SZ - TX Status FIFO Size
 *
 *  RX FIFO Size		: 16kB - LAN9118_TX_FIF_SZ
 *   RX Status FIFO Size	: RX FIFO Size / 16
 *   RX Data FIFO Size		: RX FIFO Size - RX Status FIFO Size
 */


struct lan9118_softc {
	device_t sc_dev;		/* generic device glue */
	struct ethercom sc_ec;		/* ethernet common glue */
	struct mii_data sc_mii;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	uint16_t sc_id;
	uint16_t sc_rev;
	uint8_t sc_enaddr[ETHER_ADDR_LEN];

	uint32_t sc_afc_cfg;		/* AFC_CFG configuration */
	int sc_use_extphy;
	struct callout sc_tick;

	int sc_flags;
#define LAN9118_FLAGS_SWAP	0x00000001
#define LAN9118_FLAGS_NO_EEPROM	0x00000002

	krndsource_t rnd_source;
};


int lan9118_attach(struct lan9118_softc *);
int lan9118_intr(void *);

#endif	/* _SMSC9118VAR_H_ */
