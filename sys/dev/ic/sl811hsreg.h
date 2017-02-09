/*	$NetBSD: sl811hsreg.h,v 1.4 2013/09/22 06:54:35 skrll Exp $	*/

/*
 * Not (c) 2007 Matthew Orgass
 * This file is public domain, meaning anyone can make any use of part or all
 * of this file including copying into other works without credit.  Any use,
 * modified or not, is solely the responsibility of the user.  If this file is
 * part of a collection then use in the collection is governed by the terms of
 * the collection.
 */

/*
 * ScanLogic SL811HS USB Host Controller
 */

/*
 * note: pcmcia attachment uses 4 byte port with data repeated the last three
 * bytes; using 0x2 instead of 0x1 solves bus corruption on the Vadem Clio
 * C-1000.  The main driver does not use these IDX and PORT values.
 */
#define SL11_IDX_ADDR	(0x00)
#define SL11_IDX_DATA	(0x01)
#define SL11_PORTSTART	(0x00)
#define SL11_PORTSIZE	(0x02)

#define SL11_E0BASE	(0x00)		/* Base of Control0 */
#define SL11_E0CTRL	(0x00)		/* Host Control Register */
#define SL11_E0ADDR	(0x01)		/* Host Base Address */
#define SL11_E0LEN	(0x02)		/* Host Base Length */
#define SL11_E0STAT	(0x03)		/* USB Status (Read) */
#define SL11_E0PID	SL11_E0STAT	/* Host PID, Device Endpoint (Write) */
#define SL11_E0CONT	(0x04)		/* Transfer Count (Read) */
#define SL11_E0DEV	SL11_E0CONT	/* Host Device Address (Write) */

#define SL11_E1BASE	(0x08)		/* Base of Control1 */
#define SL11_E1CTRL	(SL11_E1BASE + SL11_E0CTRL)
#define SL11_E1ADDR	(SL11_E1BASE + SL11_E0ADDR)
#define SL11_E1LEN	(SL11_E1BASE + SL11_E0LEN)
#define SL11_E1STAT	(SL11_E1BASE + SL11_E0STAT)
#define SL11_E1PID	(SL11_E1BASE + SL11_E0PID)
#define SL11_E1CONT	(SL11_E1BASE + SL11_E0CONT)
#define SL11_E1DEV	(SL11_E1BASE + SL11_E0DEV)

#define SL11_CTRL	(0x05)		/* Control Register1 */
#define SL11_IER	(0x06)		/* Interrupt Enable Register */
#define SL11_ISR	(0x0d)		/* Interrupt Status Register */
#define SL11_SOFTIME	(0x0e)		/* SOF Counter Low (Write) */
#define SL11_REV	SL11_SOFTIME	/* HW Revision Register (Read) */
#define SL811_CSOF	(0x0f)		/* SOF Counter High(R), Control2(W) */
#define SL11_MEM	(0x10)		/* Memory Buffer (0x10 - 0xff) */

#define SL11_EPCTRL_ARM		(0x01)
#define SL11_EPCTRL_ENABLE	(0x02)
#define SL11_EPCTRL_ARM_ENABLE	(SL11_EPCTRL_ARM|SL11_EPCTRL_ENABLE)
#define SL11_EPCTRL_DIRECTION	(0x04)
#define SL11_EPCTRL_ISO		(0x10)
#define SL11_EPCTRL_SOF		(0x20)
#define SL11_EPCTRL_DATATOGGLE	(0x40)
#define SL11_EPCTRL_PREAMBLE	(0x80)

#define SL11_PID_BITS	(0xf0)
#define SL11_EP_BITS	(0x0f)

#define SL11_PID_OUT    (0x10)
#define SL11_PID_IN     (0x90)
#define SL11_PID_SOF    (0x50)
#define SL11_PID_SETUP  (0xd0)

#define SLHCI_PID_SWAP_IN_OUT	(0x80) /* xor to swap IN and OUT */

#define SL11_EPSTAT_ACK		(0x01)
#define SL11_EPSTAT_ERROR	(0x02)
#define SL11_EPSTAT_TIMEOUT	(0x04)
#define SL11_EPSTAT_SEQUENCE	(0x08)
#define SL11_EPSTAT_SETUP	(0x10)
#define SL11_EPSTAT_OVERFLOW	(0x20)
#define SL11_EPSTAT_NAK		(0x40)
#define SL11_EPSTAT_STALL	(0x80)
#define SL11_EPSTAT_STATBITS	(0xf7)
#define SL11_EPSTAT_ERRBITS	(0xf6)

#define SL11_CTRL_ENABLESOF	(0x01)
/* #define SL11_CTRL_EOF2		(0x04) XXX ? Reserved in 1.5 */
#define SL11_CTRL_RESETENGINE	(0x08)
#define SL11_CTRL_JKSTATE	(0x10)
#define SL11_CTRL_LOWSPEED	(0x20)
#define SL11_CTRL_SUSPEND	(0x40)

#define SL11_IER_USBA		(0x01)	/* USB-A done */
#define SL11_IER_USBB		(0x02)	/* USB-B done */
#define SL11_IER_BABBLE		(0x04)	/* Babble detection */
#define SL11_IER_SOF		(0x10)	/* 1ms Start Of Frame timer */
#define SL11_IER_INSERT		(0x20)	/* Slave Insert/Remove detection */
#define SL11_IER_DEVDET		(0x40)	/* USB Device Detect */
#define SL11_IER_RESUME		(0x40)	/* USB Resume */
#define SLHCI_NORMAL_INTERRUPTS (0x33)	/* A, B, SOFTIMER, INSERT */

#define SL11_ISR_USBA		(0x01)	/* USB-A done */
#define SL11_ISR_USBB		(0x02)	/* USB-B done */
#define SL11_ISR_BABBLE		(0x04)	/* Babble detection or reserved */
#define SL11_ISR_RES		(0x08)	/* Reserved */
#define SL11_ISR_SOF		(0x10)	/* 1ms Start Of Frame timer */
#define SL11_ISR_INSERT		(0x20)	/* Slave Insert/Remove detection */
#define SL11_ISR_NODEV		(0x40)	/* USB Device Not Present */
#define SL11_ISR_RESUME		(0x40)	/* USB Resume */
#define SL11_ISR_DATA		(0x80)	/* Value of the Data+ pin */

#define SL11_REV_USBA		(0x01)	/* USB-A */
#define SL11_REV_USBB		(0x02)	/* USB-B */
#define SL11_REV_REVMASK	(0xf0)	/* HW Revision */

#define SL11_GET_REV(x)		((x) >> 4)
#define SLTYPE_SL11H		(0x00)	/* SL11H not supported */
#define SLTYPE_SL811HS		(0x01)
#define SLTYPE_SL811HS_R12	SLTYPE_SL811HS
#define SLTYPE_SL811HS_R14	(0x02)
#define SLTYPE_SL811HS_R15	SLTYPE_SL811HS_R14

#define SLHCI_USBAB		(0x03)	/* USB A/B bits in IER/ISR/flags */

#define SL811_CSOF_SOFMASK	(0x3f)	/* SOF High Counter */
#define SL811_CSOF_POLARITY	(0x40)	/* Change polarity */
#define SL811_CSOF_MASTER	(0x80)	/* Master/Slave selection */

#define SL11_BUFFER_START	(0x10)	/* Start of buffer memory */
#define SL11_BUFFER_END		(0xff)	/* End of buffer memory */

#define SL11_MAX_PACKET_SIZE	240

