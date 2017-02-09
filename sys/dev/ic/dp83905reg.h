/*	$NetBSD: dp83905reg.h,v 1.1 2001/12/14 10:16:03 bjh21 Exp $	*/

/*
 * Ben Harris, 2001
 *
 * This file is in the public domain.
 */

/* dp83905reg.h - NatSemi DP83905 registers */

/*
 * This file describes the special registers in the National
 * Semiconductor DP83905 AT/LANTIC AT Local Area Network Twisted-Pair
 * Interface Controller.  The Macronix MX98905 is a clone of this chip.
 *
 * The DP83905 is a DP8390 with added glue logic to enable it to
 * emulate both an NE2000 and a WD 8319.  It and its clones are
 * commonly used on podulebus Ethernet cards.
 */

/* Extra registers (in page 0) */
#define DP83905_MCRA	0x0a
#define DP83905_MCRB	0x0b

#define DP83905_MCRA_IOADDR_MASK	0x07 /* I/O Address */
#define DP83905_MCRA_IOADDR_300		0x00
#define DP83905_MCRA_IOADDR_SOFT	0x01
#define DP83905_MCRA_IOADDR_240		0x02
#define DP83905_MCRA_IOADDR_280		0x03
#define DP83905_MCRA_IOADDR_2C0		0x04
#define DP83905_MCRA_IOADDR_320		0x05
#define DP83905_MCRA_IOADDR_340		0x06
#define DP83905_MCRA_IOADDR_360		0x07
#define DP83905_MCRA_INT_MASK		0x38 /* Interrupt line used */
#define DP83905_MCRA_INT0		0x00
#define DP83905_MCRA_INT1		0x08
#define DP83905_MCRA_INT2		0x10
#define DP83905_MCRA_INT3		0x18
#define DP83905_MCRA_FREAD		0x40 /* Fast read */
#define DP83905_MCRA_MEMIO		0x80 /* Memory or I/O mode (1=>mem) */

#define DP83905_MCRB_PHY_MASK		0x03 /* Physical layer interface */
#define DP83905_MCRB_PHY_10_T		0x00 /* TPI (10BASE-T squelch) */
#define DP83905_MCRB_PHY_10_2		0x01 /* Thin Ethernet (10BASE2) */
#define DP83905_MCRB_PHY_AUI		0x02 /* Thick Ethernet (10BASE5/AUI) */
#define DP83905_MCRB_PHY_TPI_NONSPEC	0x03 /* TPI (Reduced squelch) */
#define DP83905_MCRB_GDLNK		0x04 /* Good link */
#define DP83905_MCRB_IO16CON		0x08 /* IO16* control */
#define DP83905_MCRB_CHRDY		0x10 /* CHRDY from IORD/WR* or BALE */
#define DP83905_MCRB_BE			0x20 /* Bus error */
#define DP83905_MCRB_BPWR		0x40 /* Boot PROM write */
#define DP83905_MCRB_EELOAD		0x80 /* EEPROM load */
