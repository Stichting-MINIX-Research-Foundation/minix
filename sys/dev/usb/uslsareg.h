/* $NetBSD: uslsareg.h,v 1.1 2012/01/14 21:06:01 jakllsch Exp $ */

/*
 * Copyright (c) 2011 Jonathan A. Kollasch.
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
 *
 */

#ifndef _SLSAREG_H_
#define _SLSAREG_H_

#include <lib/libkern/libkern.h>
#include <sys/systm.h>
#include <sys/types.h>

/* From Silicon Laboratories Application Note AN571 */

#define SLSA_FREQ	3686400

/* USB Control Requests */
#define SLSA_R_IFC_ENABLE	0x00
#define SLSA_R_SET_BAUDDIV	0x01
#define SLSA_R_GET_BAUDDIV	0x02
#define SLSA_R_SET_LINE_CTL	0x03
#define SLSA_R_GET_LINE_CTL	0x04
#define SLSA_R_SET_BREAK	0x05
#define SLSA_R_IMM_CHAR		0x06
#define SLSA_R_SET_MHS		0x07
#define SLSA_R_GET_MDMSTS	0x08
#define SLSA_R_SET_XON		0x09
#define SLSA_R_SET_XOFF		0x0a
#define SLSA_R_SET_EVENTMASK	0x0b
#define SLSA_R_GET_EVENTMASK	0x0c
#define SLSA_R_SET_CHAR		0x0d
#define SLSA_R_GET_CHARS	0x0e
#define SLSA_R_GET_PROPS	0x0f
#define SLSA_R_GET_COMM_STATUS	0x10
#define SLSA_R_RESET		0x11
#define SLSA_R_PURGE		0x12
#define SLSA_R_SET_FLOW		0x13
#define SLSA_R_GET_FLOW		0x14
#define SLSA_R_EMBED_EVENTS	0x15
#define SLSA_R_SET_CHARS	0x19
#define SLSA_R_GET_BAUDRATE	0x1d
#define SLSA_R_SET_BAUDRATE	0x1e
#define SLSA_R_VENDOR_SPECIFIC	0xff


#define SLSA_RV_IFC_ENABLE_DISABLE	0x0000
#define SLSA_RV_IFC_ENABLE_ENABLE	0x0001


#define SLSA_RV_BAUDDIV(b)	(SLSA_FREQ/(b))


#define SLSA_RV_LINE_CTL_STOP		__BITS(3,0)
#define SLSA_RV_LINE_CTL_PARITY		__BITS(7,4)
#define SLSA_RV_LINE_CTL_LEN		__BITS(15,8)

#define SLSA_RV_LINE_CTL_STOP_1		__SHIFTIN(0, SLSA_RV_LINE_CTL_STOP)
#define SLSA_RV_LINE_CTL_STOP_1_5	__SHIFTIN(1, SLSA_RV_LINE_CTL_STOP)
#define SLSA_RV_LINE_CTL_STOP_2		__SHIFTIN(2, SLSA_RV_LINE_CTL_STOP)

#define SLSA_RV_LINE_CTL_PARITY_NONE	__SHIFTIN(0, SLSA_RV_LINE_CTL_PARITY)
#define SLSA_RV_LINE_CTL_PARITY_ODD	__SHIFTIN(1, SLSA_RV_LINE_CTL_PARITY)
#define SLSA_RV_LINE_CTL_PARITY_EVEN	__SHIFTIN(2, SLSA_RV_LINE_CTL_PARITY)
#define SLSA_RV_LINE_CTL_PARITY_MARK	__SHIFTIN(3, SLSA_RV_LINE_CTL_PARITY)
#define SLSA_RV_LINE_CTL_PARITY_SPACE	__SHIFTIN(4, SLSA_RV_LINE_CTL_PARITY)

#define	SLSA_RV_LINE_CTL_LEN_5		__SHIFTIN(5, SLSA_RV_LINE_CTL_LEN)
#define	SLSA_RV_LINE_CTL_LEN_6		__SHIFTIN(6, SLSA_RV_LINE_CTL_LEN)
#define	SLSA_RV_LINE_CTL_LEN_7		__SHIFTIN(7, SLSA_RV_LINE_CTL_LEN)
#define	SLSA_RV_LINE_CTL_LEN_8		__SHIFTIN(8, SLSA_RV_LINE_CTL_LEN)


#define SLSA_RV_SET_BREAK_DISABLE	0x0000
#define SLSA_RV_SET_BREAK_ENABLE	0x0001


#define SLSA_RV_SET_MHS_DTR		__BIT(0)
#define SLSA_RV_SET_MHS_RTS		__BIT(1)
/* AN571 calls these next two masks, they're more like change-enables */
#define SLSA_RV_SET_MHS_DTR_MASK	__BIT(8)
#define SLSA_RV_SET_MHS_RTS_MASK	__BIT(9)


#define SLSA_RL_GET_MDMSTS	1
/* data in uint8_t returned from GET_MDMSTS */
#define SLSA_MDMSTS_DTR		__BIT(0)
#define SLSA_MDMSTS_RTS		__BIT(1)
#define SLSA_MDMSTS_CTS		__BIT(4)
#define SLSA_MDMSTS_DSR		__BIT(5)
#define SLSA_MDMSTS_RI		__BIT(6)
#define SLSA_MDMSTS_DCD		__BIT(7)


#define SLSA_RL_SET_EVENTMASK	0
#define SLSA_RL_GET_EVENTMASK	2
#define SLSA_RL_GET_EVENTSTATE	2
#define SLSA_EVENT_TERI	__BIT(0)	/* RI trailing edge */
#define SLSA_EVENT_RB80	__BIT(2)	/* Rx buf 80% full */
#define SLSA_EVENT_CR	__BIT(8)	/* char received */
#define SLSA_EVENT_SCR	__BIT(9)	/* special char received */
#define SLSA_EVENT_TQE	__BIT(10)	/* Tx queue empty */
#define SLSA_EVENT_DCTS	__BIT(11)	/* CTS changed */
#define SLSA_EVENT_DDSR	__BIT(12)	/* DSR changed */
#define SLSA_EVENT_DDCD	__BIT(13)	/* DCD changed */
#define SLSA_EVENT_BI	__BIT(14)	/* line break received */
#define SLSA_EVENT_LSE	__BIT(15)	/* line status error */


/* USETW2(wValue, char, index) */
#define SLSA_RV_SET_CHAR_EofChar	0
#define SLSA_RV_SET_CHAR_ErrorChar	1
#define SLSA_RV_SET_CHAR_BreakChar	2
#define SLSA_RV_SET_CHAR_EventChar	3
#define SLSA_RV_SET_CHAR_XonChar	4
#define SLSA_RV_SET_CHAR_XoffChar	5


#define SLSA_RV_PURGE_TX	__BIT(0)
#define SLSA_RV_PURGE_RX	__BIT(1)
#define SLSA_RV_PURGE_TX1	__BIT(2)	/* what's the second set for? */
#define SLSA_RV_PURGE_RX1	__BIT(3)


/* Communication Properties Response  Table 7. */
struct slsa_cpr {
	uint16_t	wLength;
	uint16_t	bcdVersion;
	uint32_t	ulServiceMask;
	uint32_t	_reserved8;
	uint32_t	ulMaxTxQueue;
	uint32_t	ulMaxRxQueue;
	uint32_t	ulMaxBaud;
	uint32_t	ulProvSubType;
	uint32_t	ulProvCapabilities;
	uint32_t	ulSettableParams;
	uint32_t	ulSettableBaud;
	uint16_t	wSettableData;
	uint16_t	_reserved42;
	uint32_t	ulCurrentTxQueue;
	uint32_t	ulCurrentRxQueue;
	uint32_t	_reserved52;
	uint32_t	_reserved56;
	uint16_t	uniProvName[0];
};
CTASSERT(offsetof(struct slsa_cpr, _reserved8) == 8);
CTASSERT(offsetof(struct slsa_cpr, _reserved42) == 42);
CTASSERT(offsetof(struct slsa_cpr, uniProvName[0]) == 60);
#define SLSA_CPR_MINLEN		60

#define SLSA_RL_GET_COMM_STATUS	19
/* Serial Status Response   Table 8. */
struct slsa_ssr {
	uint32_t	ulErrors;
	uint32_t	ulHoldReasons;
	uint32_t	ulAmountInInQueue;
	uint32_t	ulAmountInOutQueue;
	uint8_t 	bEofReceived;
	uint8_t 	bWaitForImmediate;
	uint8_t 	bReserved;
};
CTASSERT(offsetof(struct slsa_ssr, bReserved) == 18);
CTASSERT(sizeof(struct slsa_ssr) >= SLSA_RL_GET_COMM_STATUS);

#define SLSA_RL_SET_FLOW	16
#define SLSA_RL_GET_FLOW	16
/* Flow Control State  Setting/Response  Table 9. */
struct slsa_fcs {
	uint32_t	ulControlHandshake;
#define SERIAL_DTR_MASK		__BITS(0, 1)
#define SERIAL_CTS_HANDSHAKE	__BIT(3)
#define SERIAL_DSR_HANDSHAKE	__BIT(4)
#define SERIAL_DCD_HANDSHAKE	__BIT(5)
#define SERIAL_DSR_SENSITIVITY	__BIT(6)
	uint32_t	ulFlowReplace;
#define SERIAL_AUTO_TRANSMIT	__BIT(0)
#define SERIAL_AUTO_RECEIVE	__BIT(1)
#define SERIAL_ERROR_CHAR	__BIT(2)
#define SERIAL_NULL_STRIPPING	__BIT(3)
#define SERIAL_BREAK_CHAR	__BIT(4)
#define SERIAL_RTS_MASK		__BITS(6, 7)
#define SERIAL_XOFF_CONTINUE	__BIT(31)
	uint32_t	ulXonLimit;
	uint32_t	ulXoffLimit;
};
CTASSERT(sizeof(struct slsa_fcs) == SLSA_RL_SET_FLOW);
CTASSERT(sizeof(struct slsa_fcs) == SLSA_RL_GET_FLOW);

#define SLSA_RL_SET_CHARS	6
#define SLSA_RL_GET_CHARS	6
/* Special Characters Response  Table 12. */
struct slsa_scr {
	uint8_t 	bEofChar;
	uint8_t 	bErrorChar;
	uint8_t 	bBreakChar;
	uint8_t 	bEventChar;
	uint8_t 	bXonChar;
	uint8_t 	bXoffChar;
};
CTASSERT(sizeof(struct slsa_scr) == SLSA_RL_SET_CHARS);
CTASSERT(sizeof(struct slsa_scr) == SLSA_RL_GET_CHARS);


#define SLSA_RV_VENDOR_SPECIFIC_READ_LATCH	0x00c2
#define SLSA_RL_VENDOR_SPECIFIC_READ_LATCH	1

#define SLSA_RV_VENDOR_SPECIFIC_WRITE_LATCH	0x37e1
/*
 * on CP2103/CP2104 the latch state and latch mask are
 * written in the high and low bytes of wIndex respectively
 *
 * on CP2105, wIndex is the interface number, and the same
 * latch/mask is written as data instead.
 */
#define SLSA_RL_VENDOR_SPECIFIC_WRITE_LATCH_CP2103	0
#define SLSA_RL_VENDOR_SPECIFIC_WRITE_LATCH_CP2105	2

#endif /* _SLSAREG_H_ */
