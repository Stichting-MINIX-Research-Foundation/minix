/*	$NetBSD: ubsavar.h,v 1.9 2011/12/23 00:51:45 jakllsch Exp $	*/
/*-
 * Copyright (c) 2002, Alexander Kabaev <kan.FreeBSD.org>.
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
 */
/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ichiro FUKUHARA (ichiro@ichiro.org).
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	UBSA_MODVER		1	/* module version */

#define	UBSA_DEFAULT_CONFIG_INDEX	0
#define	UBSA_IFACE_INDEX_OFFSET	0

#define	UBSA_INTR_INTERVAL	100	/* ms */

#define	UBSA_SET_BAUDRATE  	0x00
#define	UBSA_SET_STOP_BITS	0x01
#define	UBSA_SET_DATA_BITS	0x02
#define	UBSA_SET_PARITY		0x03
#define	UBSA_SET_DTR		0x0A
#define	UBSA_SET_RTS		0x0B
#define	UBSA_SET_BREAK		0x0C
#define	UBSA_SET_FLOW_CTRL	0x10

#define UBSA_QUADUMTS_SET_PIN   0x22

#define	UBSA_PARITY_NONE	0x00
#define	UBSA_PARITY_EVEN	0x01
#define	UBSA_PARITY_ODD		0x02
#define	UBSA_PARITY_MARK	0x03
#define	UBSA_PARITY_SPACE	0x04

#define	UBSA_FLOW_NONE		0x0000
#define	UBSA_FLOW_OCTS		0x0001
#define	UBSA_FLOW_ODSR		0x0002
#define	UBSA_FLOW_IDSR		0x0004
#define	UBSA_FLOW_IDTR		0x0008
#define	UBSA_FLOW_IRTS		0x0010
#define	UBSA_FLOW_ORTS		0x0020
#define	UBSA_FLOW_UNKNOWN	0x0040
#define	UBSA_FLOW_OXON		0x0080
#define	UBSA_FLOW_IXON		0x0100

/* line status register */
#define	UBSA_LSR_TSRE		0x40	/* Transmitter empty: byte sent */
#define	UBSA_LSR_TXRDY		0x20	/* Transmitter buffer empty */
#define	UBSA_LSR_BI		0x10	/* Break detected */
#define	UBSA_LSR_FE		0x08	/* Framing error: bad stop bit */
#define	UBSA_LSR_PE		0x04	/* Parity error */
#define	UBSA_LSR_OE		0x02	/* Overrun, lost incoming byte */
#define	UBSA_LSR_RXRDY		0x01	/* Byte ready in Receive Buffer */
#define	UBSA_LSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

/* modem status register */
/* All deltas are from the last read of the MSR. */
#define	UBSA_MSR_DCD		0x80	/* Current Data Carrier Detect */
#define	UBSA_MSR_RI		0x40	/* Current Ring Indicator */
#define	UBSA_MSR_DSR		0x20	/* Current Data Set Ready */
#define	UBSA_MSR_CTS		0x10	/* Current Clear to Send */
#define	UBSA_MSR_DDCD		0x08	/* DCD has changed state */
#define	UBSA_MSR_TERI		0x04	/* RI has toggled low to high */
#define	UBSA_MSR_DDSR		0x02	/* DSR has changed state */
#define	UBSA_MSR_DCTS		0x01	/* CTS has changed state */

#define UBSA_MAXCONN		3

struct	ubsa_softc {
	device_t		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* USB device */
	usbd_interface_handle	sc_iface[UBSA_MAXCONN]; /* interface */

	int			sc_iface_number[UBSA_MAXCONN];	/* interface number */
	int			sc_config_index;	/* USB CONFIG_INDEX */

	int			sc_intr_number;	/* interrupt number */
	usbd_pipe_handle	sc_intr_pipe;	/* interrupt pipe */
	u_char			*sc_intr_buf;	/* interrupt buffer */
	int			sc_isize;

	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */

	u_char			sc_lsr;		/* Local status register */
	u_char			sc_msr;		/* ubsa status register */

	device_t		sc_subdevs[UBSA_MAXCONN]; /* ucom device */
	int			sc_numif;	/* number of interfaces */

	u_char			sc_dying;	/* disconnecting */
	u_char			sc_quadumts;
	u_int16_t		sc_devflags;		
};


void ubsa_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

void ubsa_get_status(void *, int, u_char *, u_char *);
void ubsa_set(void *, int, int, int);
int  ubsa_param(void *, int, struct termios *);
int  ubsa_open(void *, int);
void ubsa_close(void *, int);

void ubsa_break(struct ubsa_softc *sc, int, int onoff);
int  ubsa_request(struct ubsa_softc *, int, u_int8_t, u_int16_t);
void ubsa_dtr(struct ubsa_softc *, int, int);
void ubsa_quadumts_dtr(struct ubsa_softc *, int, int);
void ubsa_rts(struct ubsa_softc *, int, int);
void ubsa_quadumts_rts(struct ubsa_softc *, int, int);
void ubsa_baudrate(struct ubsa_softc *, int, speed_t);
void ubsa_parity(struct ubsa_softc *, int, tcflag_t);
void ubsa_databits(struct ubsa_softc *, int, tcflag_t);
void ubsa_stopbits(struct ubsa_softc *, int, tcflag_t);
void ubsa_flow(struct ubsa_softc *, int, tcflag_t, tcflag_t);
