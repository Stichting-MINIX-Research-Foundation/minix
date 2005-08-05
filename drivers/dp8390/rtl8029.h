/*
rtl8029.h

Created:	Sep 2003 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

/* Bits in dp_cr */
#define CR_PS_P3	0xC0	/* Register Page 3                   */

#define inb_reg3(dep, reg)	(inb (dep->de_dp8390_port+reg))
#define outb_reg3(dep, reg, data) (outb(dep->de_dp8390_port+reg, data))

/*
 * $PchId: rtl8029.h,v 1.3 2004/08/03 15:11:06 philip Exp $
 */
