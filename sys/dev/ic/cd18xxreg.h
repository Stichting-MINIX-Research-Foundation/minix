/*	$NetBSD: cd18xxreg.h,v 1.4 2008/05/29 14:51:27 mrg Exp $	*/

/*
 * Copyright (c) 1998, 2001 Matthew R. Green
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * cirrus-logic CL-CD180/CD1864/CD1865 register definitions, from the
 * CL-CD1865 data book.
 */


/*
 * available registers for us.
 *
 * the cd1865 provides 4 types of registers:  global, indexed indirect,
 * channel, and unavailable.  we should never touch the unavailable, as it
 * may cause the cd1865 to fail.  the indexed indirect registers are
 * really pointers to the correct channel we are currently servicing, and
 * as such must only be accessed during service-request service routines.
 * global registers set and provide common functionality between all of
 * the channels.  channel registers only affect the specific channel.
 * access to channel registers is limited to the current channel, as
 * specified in the CAR register, ie. to access different channels, the CAR
 * register must be changed first.
 */


/*
 * the registers themselves.
 */

/* global registers */
#define	CD18xx_GFRCR		0x6b	/* global firmware revision code */
#define	CD18xx_SRCR		0x66	/* service request configuration */
#define	CD18xx_PPRH		0x70	/* prescaler period (high) */
#define	CD18xx_PPRL		0x71	/* prescaler period (low) */
#define	CD18xx_MSMR		0x61	/* modem service match */
#define	CD18xx_TSMR		0x62	/* transmit service match */
#define	CD18xx_RSMR		0x63	/* receive service match */
#define	CD18xx_GSVR		0x40	/* global service vector */
#define	CD18xx_SRSR		0x65	/* service request status */
#define	CD18xx_MRAR		0x75	/* modem request acknowledge */
#define	CD18xx_TRAR		0x76	/* transmit request acknowledge */
#define	CD18xx_RRAR		0x77	/* receive request acknowledge */
#define	CD18xx_GSCR1		0x41	/* global service channel (1) */
#define	CD18xx_GSCR2		0x42	/* global service channel (2) */
#define	CD18xx_GSCR3		0x43	/* global service channel (3) */
#define	CD18xx_CAR		0x64	/* channel access register */

/* indexed indirect registers */
#define	CD18xx_RDCR		0x07	/* receive data count */
#define	CD18xx_RDR		0x78	/* receiver data register */
#define	CD18xx_RCSR		0x7a	/* receiver channel status */
#define	CD18xx_TDR		0x7b	/* transmit data register */
#define	CD18xx_EOSRR		0x7f	/* end of service request */

/* channel registers */
#define	CD18xx_SRER		0x02	/* service request enable */
#define	CD18xx_CCR		0x01	/* channel command */
#define	CD18xx_COR1		0x03	/* channel option (1) */
#define	CD18xx_COR2		0x04	/* channel option (2) */
#define	CD18xx_COR3		0x05	/* channel option (3) */
#define	CD18xx_CCSR		0x06	/* channel control status */
#define	CD18xx_RBR		0x33	/* receiver bit */
#define	CD18xx_RTPR		0x18	/* receive time-out period */
#define	CD18xx_RBPRH		0x31	/* receive bit rate period (high) */
#define	CD18xx_RBPRL		0x32	/* receive bit rate period (low) */
#define	CD18xx_TBPRH		0x39	/* transmit bit rate period (high) */
#define	CD18xx_TBPRL		0x3a	/* transmit bit rate period (low) */
#define	CD18xx_SCHR1		0x09	/* special character (1) */
#define	CD18xx_SCHR2		0x0a	/* special character (2) */
#define	CD18xx_SCHR3		0x0b	/* special character (3) */
#define	CD18xx_SCHR4		0x0c	/* special character (4) */
#define	CD18xx_MCR		0x10	/* modem change */
#define	CD18xx_MCOR1		0x10	/* modem change option (1) */
#define	CD18xx_MCOR2		0x11	/* modem change option (2) */
#define	CD18xx_MSVR		0x28	/* modem signal value */
#define	CD18xx_MSVRTS		0x29	/* modem signal value RTS */
#define	CD18xx_MSVDTR		0x2a	/* mdoem signal value DTR */


/*
 * inside the registers
 */

/* global registers */

/* global firmware revision code */
#define	CD180_GFRCR_REV_B	0x81	/* CL-CD180B */
#define	CD180_GFRCR_REV_C	0x82	/* CL-CD180C */
#define	CD1864_GFRCR_REVISION_A	0x82	/* CL-CD1864A */
#define	CD1865_GFRCR_REVISION_A	0x83	/* CL-CD1865A */
#define	CD1865_GFRCR_REVISION_B	0x84	/* CL-CD1865B */
#define	CD1865_GFRCR_REVISION_C	0x85	/* CL-CD1865C */

/* service request configuration register */
#define	CD18xx_SRCR_PKGTYP	0x80	/* package type (RO) */
#define	CD18xx_SRCR_REGACKEN	0x40	/* enable register acks */
#define	CD18xx_SRCR_DAISYEN	0x20	/* enable daisy-chain */
#define	CD18xx_SRCR_GLOBPRI	0x10	/* global priority */
#define	CD18xx_SRCR_UNFAIR	0x08	/* unfair override */
#define	CD18xx_SRCR_AUTOPRI	0x02	/* auto prioritizing */
#define	CD18xx_SRCR_PRISEL	0x01	/* priority selection */

/* global service vector register */
#define	CD18xx_GSVR_CLEAR	0x00	/* clear GSVR for reset */
#define	CD18xx_GSVR_READY	0xff	/* modem is ready */
#define	CD18xx_GSVR_IDMASK	0xf8	/* unique ID per-chip */
#define	CD18xx_GSVR_SETID(sc)	((((sc)->sc_chip_id & ~1) << 5) | \
				 (((sc)->sc_chip_id & 1) << 3))
#define	CD18xx_GSVR_GROUPTYPE	0x07	/* group/type */
#define	CD18xx_GSVR_NOREQPEND	0x00	/* no request pending */
#define	CD18xx_GSVR_MODEM	0x01	/* modem signal change */
#define	CD18xx_GSVR_TXDATA	0x02	/* tx data */
#define	CD18xx_GSVR_RXDATA	0x03	/* rx good data */
#define	CD18xx_GSVR_RXEXCEPTION	0x07	/* request exception */
#define	CD18xx_GSVR_RXINTR(x)	\
	(((x) & CD18xx_GSVR_GROUPTYPE) == CD18xx_GSVR_RXDATA || \
	 ((x) & CD18xx_GSVR_GROUPTYPE) == CD18xx_GSVR_RXEXCEPTION)
#define	CD18xx_GSVR_TXINTR(x)	\
	(((x) & CD18xx_GSVR_GROUPTYPE) == CD18xx_GSVR_TXDATA)
#define	CD18xx_GSVR_MXINTR(x)	\
	(((x) & CD18xx_GSVR_GROUPTYPE) == CD18xx_GSVR_MODEM)

/* service request status register */
#define	CD18xx_SRSR_CONTEXT	0xc0	/* service request context */
#define	CD18xx_SRSR_PENDING	0x15	/* get status bits for each */
#define	CD18xx_SRSR_RxPEND	0x10	/* got a Rx interrupt */
#define	CD18xx_SRSR_TxPEND	0x04	/* got a Tx interrupt */
#define	CD18xx_SRSR_MxPEND	0x01	/* got a modem interrupt */

/* global service channel registers */
#define	CD18xx_GSCR_USER1	0xe0	/* 3 bits of user-defined data */
#define	CD18xx_GSCR_CAR		0x1c	/* CAR of current channel */
#define	CD18xx_GSCR_USER2	0x03	/* 2 bits of user-defined data */

/* indexed indirect registers */

/* receive data count register */
#define	CD18xx_RDCR_ZERO	0xf0	/* reserved, must be zero */
#define	CD18xx_RDCR_GOODBYTES	0x0f	/* number of good bytes */

/* receive character status register */
#define	CD18xx_RCSR_TIMEOUT	0x80	/* timeout has occurred on channel */
#define	CD18xx_RCSR_SCD		0x70	/* special character detect */
#define	CD18xx_RCSR_BREAK	0x08	/* line break detected */
#define	CD18xx_RCSR_PARITYERR	0x04	/* parity error detected */
#define	CD18xx_RCSR_FRAMERR	0x02	/* framing error detected */
#define	CD18xx_RCSR_OVERRUNERR	0x01	/* overrun error detected */

/* transmit data register */
#define CD18xx_TDR_ETC_BYTE	0x00	/* first byte of break message */
#define CD18xx_TDR_BREAK_BYTE	0x81	/* first byte of break message */
#define CD18xx_TDR_NOBREAK_BYTE	0x83	/* first byte of clean break message */

/* channel registers */

/* service request enable register */
#define	CD18xx_SRER_DSR		0x80	/* DSR service request */
#define	CD18xx_SRER_CD		0x40	/* CD service request */
#define	CD18xx_SRER_CTS		0x20	/* CTS service request */
#define	CD18xx_SRER_Rx		0x10	/* Rx data service request */
#define	CD18xx_SRER_RxSC	0x08	/* Rx special char service request */
#define	CD18xx_SRER_Tx		0x04	/* Tx ready service request */
#define	CD18xx_SRER_TxEMPTY	0x02	/* Tx empty service request */
#define	CD18xx_SRER_NNDT	0x01	/* no new data timeout service request */

/* channel command register */
#define	CD18xx_CCR_RESET	0x80	/* reset channel command */
#define	CD18xx_CCR_CORCHG	0x40	/* COR change command */
#define	CD18xx_CCR_SENDSC	0x20	/* send special character command */
#define	CD18xx_CCR_CHANCTL	0x10	/* channel control command */

/* bits inside CCR's least significant half-byte */
#define	CD18xx_CCR_RESET_HARD	0x01	/* full, hard reset */
#define	CD18xx_CCR_RESET_CHAN	0x00	/* reset only the current channel */
#define	CD18xx_CCR_CORCHG_COR3	0x08	/* change COR3 command */
#define	CD18xx_CCR_CORCHG_COR2	0x04	/* change COR2 command */
#define	CD18xx_CCR_CORCHG_COR1	0x02	/* change COR1 command */
#define	CD18xx_CCR_SENDSC_SEND1	0x01	/* send SC 1, or 1&3 */
#define	CD18xx_CCR_SENDSC_SEND2	0x02	/* send SC 2, or 2&4 */
#define	CD18xx_CCR_SENDSC_SEND3	0x03	/* send SC 3 */
#define	CD18xx_CCR_SENDSC_SEND4	0x04	/* send SC 4 */
/* note that these are slower than enabling/disabling SRER */
#define	CD18xx_CCR_CHANCTL_TxEN	0x08	/* transmitter enable */
#define	CD18xx_CCR_CHANCTL_TxDI	0x04	/* transmitter disable */
#define	CD18xx_CCR_CHANCTL_RxEN	0x02	/* receiver enable */
#define	CD18xx_CCR_CHANCTL_RxDI	0x01	/* receiver disable */

/* channel option register 1 */
#define	CD18xx_COR1_PARITY	0x80		/* parity */
#define	CD18xx_COR1_PARITY_ODD		0x80	/* odd parity */
#define	CD18xx_COR1_PARITY_EVEN		0x00	/* even parity */
#define	CD18xx_COR1_PARITY_MODE	0x60		/* parity mode */
#define	CD18xx_COR1_PARITY_NONE		0x00	/* no parity */
#define	CD18xx_COR1_PARITY_FORCE	0x20	/* force parity */
#define	CD18xx_COR1_PARITY_NORMAL	0x40	/* normal parity */
#define	CD18xx_COR1_IGNORE	0x10		/* parity ignore mode */
#define	CD18xx_COR1_STOPBITLEN	0x0c		/* stop bit length */
#define	CD18xx_COR1_STOPBIT_1		0x00	/* 1 stop bit */
#define	CD18xx_COR1_STOPBIT_1_5		0x04	/* 1.5 stop bits */
#define	CD18xx_COR1_STOPBIT_2		0x08	/* 2 stop bits */
#define	CD18xx_COR1_STOPBIT_2_5		0x0c	/* 2.5 stop bits */
#define	CD18xx_COR1_CHARLEN	0x03		/* character length */
#define CD18xx_COR1_CS5			0x00	/* 5 bit chars */
#define CD18xx_COR1_CS6			0x01	/* 7 bit chars */
#define CD18xx_COR1_CS7			0x02	/* 7 bit chars */
#define CD18xx_COR1_CS8			0x03	/* 8 bit chars */

/* channel option register 2 */
#define	CD18xx_COR2_IXM		0x80	/* implied XON mode */
#define	CD18xx_COR2_TxIBE	0x40	/* Tx inband flow control auto enable */
#define	CD18xx_COR2_ETC		0x20	/* embedded Tx command enable */
#define	CD18xx_COR2_LLM		0x10	/* local loopback mode */
#define	CD18xx_COR2_RLM		0x08	/* remote loopback mode */
#define	CD18xx_COR2_RTSAOE	0x04	/* RTS auto output enable */
#define	CD18xx_COR2_CTSAE	0x02	/* CTS auto enable */
#define	CD18xx_COR2_DSRAE	0x01	/* DSR auto enable */

/* channel option register 3 */
#define	CD18xx_COR3_XONCH	0x80	/* XON character definition */
#define	CD18xx_COR3_XOFFCH	0x40	/* XOFF character definition */
#define	CD18xx_COR3_FCTM	0x20	/* flow control transparency mode */
#define	CD18xx_COR3_SCDE	0x10	/* special character detection enable */
#define	CD18xx_COR3_FIFOTHRESH	0x08	/* Rx FIFO threshold */

/* channel control status register */
#define	CD18xx_CCSR_RxEN	0x80	/* Rx enable */
#define	CD18xx_CCSR_RxFLOFF	0x40	/* Rx flow control off enable */
#define	CD18xx_CCSR_RxFLON	0x20	/* Rx flow control on enable */
#define	CD18xx_CCSR_TxEN	0x08	/* Tx enable */
#define	CD18xx_CCSR_TxFLOFF	0x04	/* Tx flow control off enable */
#define	CD18xx_CCSR_TxFLON	0x02	/* Tx flow control on enable */

/* receiver bit register */
#define	CD18xx_RBR_RxD		0x40	/* last RxD input */
#define	CD18xx_RBR_STARTHUNT	0x20	/* hunting for a start bit */

/* bit rate period resisters */
#define CD18xx_xBRPR_TPC	0x10	/* ticks per character */

/* mode change register */
#define	CD18xx_MCR_DSR		0x80	/* DSR changed */
#define	CD18xx_MCR_CD		0x40	/* CD changed */
#define	CD18xx_MCR_CTS		0x20	/* CST changed */

/* modem change option register 1 */
#define	CD18xx_MCOR1_DSR	0x80	/* high-to-low on DSR */
#define	CD18xx_MCOR1_CD		0x40	/* high-to-low on CD */
#define	CD18xx_MCOR1_CTS	0x20	/* high-to-low on CTS */
#define	CD18xx_MCOR1_DTR	0x08	/* high-to-low on DSR mode */

/* modem change option register 2 */
#define	CD18xx_MCOR2_DSR	0x80	/* low-to-high on DSR */
#define	CD18xx_MCOR2_CD		0x40	/* low-to-high on CD */
#define	CD18xx_MCOR2_CTS	0x20	/* low-to-high on CST */

/* modem signal value register */
#define	CD18xx_MSVR_DSR		0x80	/* current DSR state */
#define	CD18xx_MSVR_CD		0x40	/* current CD state */
#define	CD18xx_MSVR_CTS		0x20	/* current CTS state */
#define	CD18xx_MSVR_DTR		0x02	/* current DTR state */
#define	CD18xx_MSVR_RTS		0x01	/* current RTS state */
#define	CD18xx_MSVR_RESET	(CD18xx_MSVR_DSR|CD18xx_MSVR_CD| \
				 CD18xx_MSVR_CTS|CD18xx_MSVR_DTR| \
				 CD18xx_MSVR_RTS)

/* modem signal value request-to-send register */
#define	CD18xx_MSVRTS_RTS	0x01	/* change RTS and not DTR */

/* modem signal value data-terminal-ready register */
#define	CD18xx_MSVDTR_DTR	0x01	/* change DTR and not RTS */

