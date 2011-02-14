/*
net/gen/vjhc.h

Defines for Van Jacobson TCP/IP Header Compression as defined in RFC-1144

Created:	Nov 15, 1993 by Philip Homburg <philip@cs.vu.nl>
*/

#ifndef __NET__GEN__VJHC_H__
#define __NET__GEN__VJHC_H__

#define VJHC_FLAG_U	0x01
#define VJHC_FLAG_W	0x02
#define VJHC_FLAG_A	0x04
#define VJHC_FLAG_S	0x08
#define VJHC_FLAG_P	0x10
#define VJHC_FLAG_I	0x20
#define VJHC_FLAG_C	0x40

#define VJHC_SPEC_I	(VJHC_FLAG_S | VJHC_FLAG_W | VJHC_FLAG_U)
#define VJHC_SPEC_D	(VJHC_FLAG_S | VJHC_FLAG_A | VJHC_FLAG_W | VJHC_FLAG_U)
#define VJHC_SPEC_MASK	(VJHC_FLAG_S | VJHC_FLAG_A | VJHC_FLAG_W | VJHC_FLAG_U)

#define VJHC_ENCODE(cp, n) \
{ \
	if ((u16_t)(n) >= 256) \
	{ \
		*(cp)++= 0; \
		*(cp)++= (n >> 8); \
		*(cp)++= (n); \
	} \
	else \
		*(cp)++= (n); \
}

#define VJHC_ENCODEZ(cp, n) \
{ \
	if ((u16_t)(n) == 0 || (u16_t)(n) >= 256) \
	{ \
		*(cp)++= 0; \
		*(cp)++= (n >> 8); \
		*(cp)++= (n); \
	} \
	else \
		*(cp)++= (n); \
}

#define VJHC_DECODEL(cp, l) \
{ \
	if (*(cp) == 0) \
	{ \
		(l)= htonl(ntohl((l)) + (((cp)[1] << 8) | (cp)[2])); \
		(cp) += 3; \
	} \
	else \
		(l)= htonl(ntohl((l)) + (u32_t)*(cp)++); \
}

#define VJHC_DECODES(cp, s) \
{ \
	if (*(cp) == 0) \
	{ \
		(s)= htons(ntohs((s)) + (((cp)[1] << 8) | (cp)[2])); \
		(cp) += 3; \
	} \
	else \
		(s)= htons(ntohs((s)) + (u16_t)*(cp)++); \
}

#define VJHC_DECODEU(cp, s) \
{ \
	if (*(cp) == 0) \
	{ \
		(s)= htons(((cp)[1] << 8) | (cp)[2]); \
		(cp) += 3; \
	} \
	else \
		(s)= htons((u16_t)*(cp)++); \
}

#endif /* __NET__GEN__VJHC_H__ */

/*
 * $PchId: vjhc.h,v 1.2 1995/11/17 22:14:46 philip Exp $
 */
