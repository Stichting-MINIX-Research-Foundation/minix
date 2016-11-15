/* $NetBSD: ppbus_1284.h,v 1.8 2009/04/05 09:56:16 cegger Exp $ */

/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *
 * FreeBSD: src/sys/dev/ppbus/ppb_1284.h,v 1.7 2000/01/14 08:03:14 nsouch Exp
 *
 */
#ifndef __PPBUS_1284_H
#define __PPBUS_1284_H

#include <sys/device.h>	/* for device_t */

/*
 * IEEE1284 signals
 */

/* host driven signals */

#define nHostClk	STROBE
#define Write		STROBE

#define nHostBusy	AUTOFEED
#define nHostAck	AUTOFEED
#define DStrb		AUTOFEED

#define nReveseRequest	nINIT

#define nActive1284	SELECTIN
#define AStrb		SELECTIN

/* peripheral driven signals */

#define nDataAvail	nFAULT
#define nPeriphRequest	nFAULT

#define Xflag		SELECT

#define AckDataReq	PERROR
#define nAckReverse	PERROR

#define nPtrBusy	nBUSY
#define nPeriphAck	nBUSY
#define Wait		nBUSY

#define PtrClk		nACK
#define PeriphClk	nACK
#define Intr		nACK

/* request mode values */
#define NIBBLE_1284_NORMAL	0x0
#define NIBBLE_1284_REQUEST_ID	0x4
#define BYTE_1284_NORMAL	0x1
#define BYTE_1284_REQUEST_ID	0x5
#define ECP_1284_NORMAL		0x10
#define ECP_1284_REQUEST_ID	0x14
#define ECP_1284_RLE		0x30
#define ECP_1284_RLE_REQUEST_ID	0x34
#define EPP_1284_NORMAL		0x40
#define EXT_LINK_1284_NORMAL	0x80

/* ieee1284 mode options */
#define PPBUS_REQUEST_ID		0x1
#define PPBUS_USE_RLE		0x2
#define PPBUS_EXTENSIBILITY_LINK	0x4

/* ieee1284 errors */
#define PPBUS_NO_ERROR		0
#define PPBUS_MODE_UNSUPPORTED	1	/* mode not supported by peripheral */
#define PPBUS_NOT_IEEE1284	2	/* not an IEEE1284 compliant periph. */
#define PPBUS_TIMEOUT		3	/* timeout */
#define PPBUS_INVALID_MODE	4	/* current mode is incorrect */

/* ieee1284 host side states */
#define PPBUS_ERROR			0
#define PPBUS_FORWARD_IDLE		1
#define PPBUS_NEGOTIATION			2
#define PPBUS_SETUP			3
#define PPBUS_ECP_FORWARD_IDLE		4
#define PPBUS_FWD_TO_REVERSE		5
#define PPBUS_REVERSE_IDLE		6
#define PPBUS_REVERSE_TRANSFER		7
#define PPBUS_REVERSE_TO_FWD		8
#define PPBUS_EPP_IDLE			9
#define PPBUS_TERMINATION			10

/* peripheral side states */
#define PPBUS_PERIPHERAL_NEGOTIATION	11
#define PPBUS_PERIPHERAL_IDLE		12
#define PPBUS_PERIPHERAL_TRANSFER		13
#define PPBUS_PERIPHERAL_TERMINATION	14

/* Function prototypes */

/* Host functions */
int ppbus_1284_negotiate(device_t, int, int);
int ppbus_1284_terminate(device_t);
int ppbus_1284_read_id(device_t, int, char **, size_t *, size_t *);
int ppbus_1284_get_state(device_t);
int ppbus_1284_set_state(device_t, int state);

/* Peripheral functions */
int ppbus_peripheral_terminate(device_t, int);
int ppbus_peripheral_negotiate(device_t, int, int);
int byte_peripheral_write(device_t, char *, int, int *);

#endif /* __PPBUS_1284_H */
