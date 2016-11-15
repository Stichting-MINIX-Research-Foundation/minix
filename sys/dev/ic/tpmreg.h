/*	$NetBSD: tpmreg.h,v 1.3 2012/01/23 04:12:26 christos Exp $	*/

/*
 * Copyright (c) 2008, 2009 Michael Shalayeff
 * Copyright (c) 2009, 2010 Hans-Jörg Höxer
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define	TPM_BUFSIZ	1024

#define TPM_HDRSIZE	10

#define TPM_PARAM_SIZE	0x0001

#define	TPM_ACCESS			0x0000	/* access register */
#define	TPM_ACCESS_ESTABLISHMENT	0x01	/* establishment */
#define	TPM_ACCESS_REQUEST_USE		0x02	/* request using locality */
#define	TPM_ACCESS_REQUEST_PENDING	0x04	/* pending request */
#define	TPM_ACCESS_SEIZE		0x08	/* request locality seize */
#define	TPM_ACCESS_SEIZED		0x10	/* locality has been seized */
#define	TPM_ACCESS_ACTIVE_LOCALITY	0x20	/* locality is active */
#define	TPM_ACCESS_VALID		0x80	/* bits are valid */
#define	TPM_ACCESS_BITS	\
    "\020\01EST\02REQ\03PEND\04SEIZE\05SEIZED\06ACT\010VALID"

#define	TPM_INTERRUPT_ENABLE	0x0008
#define	TPM_GLOBAL_INT_ENABLE	0x80000000	/* enable ints */
#define	TPM_CMD_READY_INT	0x00000080	/* cmd ready enable */
#define	TPM_INT_EDGE_FALLING	0x00000018
#define	TPM_INT_EDGE_RISING	0x00000010
#define	TPM_INT_LEVEL_LOW	0x00000008
#define	TPM_INT_LEVEL_HIGH	0x00000000
#define	TPM_LOCALITY_CHANGE_INT	0x00000004	/* locality change enable */
#define	TPM_STS_VALID_INT	0x00000002	/* int on TPM_STS_VALID is set */
#define	TPM_DATA_AVAIL_INT	0x00000001	/* int on TPM_STS_DATA_AVAIL is set */
#define	TPM_INTERRUPT_ENABLE_BITS \
    "\177\020b\0DRDY\0b\1STSVALID\0b\2LOCCHG\0" \
    "F\3\2:\0HIGH\0:\1LOW\0:\2RISE\0:\3FALL\0" \
    "b\7IRDY\0b\x1fGIENABLE\0"

#define	TPM_INT_VECTOR		0x000c	/* 8 bit reg for 4 bit irq vector */
#define	TPM_INT_STATUS		0x0010	/* bits are & 0x87 from TPM_INTERRUPT_ENABLE */

#define	TPM_INTF_CAPABILITIES		0x0014	/* capability register */
#define	TPM_INTF_BURST_COUNT_STATIC	0x0100	/* TPM_STS_BMASK static */
#define	TPM_INTF_CMD_READY_INT		0x0080	/* int on ready supported */
#define	TPM_INTF_INT_EDGE_FALLING	0x0040	/* falling edge ints supported */
#define	TPM_INTF_INT_EDGE_RISING	0x0020	/* rising edge ints supported */
#define	TPM_INTF_INT_LEVEL_LOW		0x0010	/* level-low ints supported */
#define	TPM_INTF_INT_LEVEL_HIGH		0x0008	/* level-high ints supported */
#define	TPM_INTF_LOCALITY_CHANGE_INT	0x0004	/* locality-change int (mb 1) */
#define	TPM_INTF_STS_VALID_INT		0x0002	/* TPM_STS_VALID int supported */
#define	TPM_INTF_DATA_AVAIL_INT		0x0001	/* TPM_STS_DATA_AVAIL int supported (mb 1) */
#define	TPM_CAPSREQ \
  (TPM_INTF_DATA_AVAIL_INT|TPM_INTF_LOCALITY_CHANGE_INT|TPM_INTF_INT_LEVEL_LOW)
#define	TPM_CAPBITS \
  "\020\01IDRDY\02ISTSV\03ILOCH\04IHIGH\05ILOW\06IRISE\07IFALL\010IRDY\011BCST"

#define	TPM_STS			0x0018		/* status register */
#define TPM_STS_MASK		0x000000ff	/* status bits */
#define	TPM_STS_BMASK		0x00ffff00	/* ro io burst size */
#define	TPM_STS_VALID		0x00000080	/* ro other bits are valid */
#define	TPM_STS_CMD_READY	0x00000040	/* rw chip/signal ready */
#define	TPM_STS_GO		0x00000020	/* wo start the command */
#define	TPM_STS_DATA_AVAIL	0x00000010	/* ro data available */
#define	TPM_STS_DATA_EXPECT	0x00000008	/* ro more data to be written */
#define	TPM_STS_RESP_RETRY	0x00000002	/* wo resend the response */
#define	TPM_STS_BITS	"\020\010VALID\07RDY\06GO\05DRDY\04EXPECT\02RETRY"

#define	TPM_DATA	0x0024
#define	TPM_ID		0x0f00
#define	TPM_REV		0x0f04
#define	TPM_SIZE	0x5000		/* five pages of the above */

#define	TPM_ACCESS_TMO	2000		/* 2sec */
#define	TPM_READY_TMO	2000		/* 2sec */
#define	TPM_READ_TMO	2000		/* 2sec */
#define TPM_BURST_TMO	2000		/* 2sec */

#define	TPM_LEGACY_BUSY	0x01
#define	TPM_LEGACY_ABRT	0x01
#define	TPM_LEGACY_DA	0x02
#define	TPM_LEGACY_RE	0x04
#define	TPM_LEGACY_LAST	0x04
#define	TPM_LEGACY_BITS	"\020\01BUSY\2DA\3RE\4LAST"
#define	TPM_LEGACY_TMO		(2*60)	/* sec */
#define	TPM_LEGACY_SLEEP	5	/* ticks */
#define	TPM_LEGACY_DELAY	100
