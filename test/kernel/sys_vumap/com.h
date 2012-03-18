#ifndef _VUMAP_COM_H
#define _VUMAP_COM_H

#define VTR_RELAY	0x3000		/* SYS_VUMAP relay request */

#define VTR_VGRANT	m10_l1		/* grant for virtual vector */
#define VTR_VCOUNT	m10_i1		/* nr of elements in virtual vector */
#define VTR_OFFSET	m10_l2		/* offset into first element */
#define VTR_ACCESS	m10_i2		/* access flags (VUA_) */
#define VTR_PGRANT	m10_l3		/* grant for physical vector */
#define VTR_PCOUNT	m10_i3		/* nr of physical elements (in/out) */

#endif /* _VUMAP_COM_H */
