/*
ne2000.h

Created:	March 15, 1994 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#ifndef NE2000_H
#define NE2000_H

#define NE_DP8390	0x00
#define NE_DATA		0x10
#define NE_RESET	0x1F

#define NE1000_START	0x2000
#define NE1000_SIZE	0x2000
#define NE2000_START	0x4000
#define NE2000_SIZE	0x4000

#define inb_ne(dep, reg)	(inb(dep->de_base_port+reg))
#define outb_ne(dep, reg, data)	(outb(dep->de_base_port+reg, data))
#define inw_ne(dep, reg)	(inw(dep->de_base_port+reg))
#define outw_ne(dep, reg, data)	(outw(dep->de_base_port+reg, data))

#endif /* NE2000_H */

/*
 * $PchId: ne2000.h,v 1.4 2004/08/03 12:03:20 philip Exp $
 */
