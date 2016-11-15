/* $NetBSD: ppbus_var.h,v 1.5 2005/12/11 12:23:28 christos Exp $ */

#ifndef __PPBUS_VAR_H
#define __PPBUS_VAR_H

/* PPBUS mode masks. */
#define PPBUS_COMPATIBLE	0x01	/* Centronics compatible mode */
#define PPBUS_NIBBLE		0x02	/* reverse 4 bit mode */
#define PPBUS_PS2		0x04	/* PS/2 byte mode */
#define PPBUS_EPP		0x08	/* EPP mode, 32 bit */
#define PPBUS_ECP		0x10	/* ECP mode */
#define PPBUS_FAST		0x20	/* Fast Centronics mode */
/* mode aliases */
#define PPBUS_SPP		PPBUS_NIBBLE | PPBUS_PS2 /* Won't work! */
#define PPBUS_BYTE		PPBUS_PS2
#define PPBUS_MASK		0x3f
#define PPBUS_OPTIONS_MASK	0xc0
/* Useful macros for this field */
#define PPBUS_IS_EPP(mode) ((mode) & PPBUS_EPP)
#define PPBUS_IN_EPP_MODE(bus) (PPBUS_IS_EPP(ppbus_get_mode(bus)))
#define PPBUS_IN_NIBBLE_MODE(bus) (ppbus_get_mode(bus) & PPBUS_NIBBLE)
#define PPBUS_IN_PS2_MODE(bus) (ppbus_get_mode(bus) & PPBUS_PS2)

/* PPBUS capabilities */
#define PPBUS_HAS_INTR		0x01	/* Interrupt available */
#define PPBUS_HAS_DMA		0x02	/* DMA available */
#define PPBUS_HAS_FIFO		0x04	/* FIFO available */
#define PPBUS_HAS_PS2		0x08	/* PS2 mode capable */
#define PPBUS_HAS_ECP		0x10	/* ECP mode available */
#define PPBUS_HAS_EPP		0x20	/* EPP mode available */

/* IEEE flag in soft config */
#define PPBUS_DISABLE_IEEE	0x00
#define PPBUS_ENABLE_IEEE	0x01

/* List of IVARS available to ppbus device drivers */
/* #define PPBUS_IVAR_MODE 0 */
#define PPBUS_IVAR_DMA		1
#define PPBUS_IVAR_INTR		2
#define PPBUS_IVAR_EPP_PROTO    3
#define PPBUS_IVAR_IEEE		4
/* Needed by callback's implemented using callout */
#define PPBUS_IVAR_IRQSTAT	5
#define PPBUS_IVAR_DMASTAT	6
/* other fields are reserved to the ppbus internals */

/* EPP protocol versions */
#define PPBUS_EPP_1_9           0x0                     /* default */
#define PPBUS_EPP_1_7           0x1

/* Parallel Port Bus sleep/wakeup queue. */
#define PPBUSPRI		(PZERO+8)

#endif /* __PPBUS_VAR_H */
