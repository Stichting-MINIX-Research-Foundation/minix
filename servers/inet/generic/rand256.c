/*
rand256.c

Created:	Oct 2000 by Philip Homburg <philip@f-mnx.phicoh.com>

Generate 256-bit random numbers 
*/

#include <sha2.h>
#include "inet.h"
#include "rand256.h"

PRIVATE u32_t base_bits[8];

PUBLIC void init_rand256(bits)
u8_t bits[32];
{
	memcpy(base_bits, bits, sizeof(base_bits));
}

PUBLIC void rand256(bits)
u8_t bits[32];
{
	u32_t a;
	SHA256_CTX ctx;

	a= ++base_bits[0];
	if (a == 0)
		base_bits[1]++;
	SHA256_Init(&ctx);
	SHA256_Update(&ctx, (unsigned char *)base_bits, sizeof(base_bits));
	SHA256_Final(bits, &ctx);
}

/*
 * $PchId: rand256.c,v 1.1 2005/06/28 14:13:43 philip Exp $
 */
