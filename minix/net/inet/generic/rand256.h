/*
rand256.h

Created:	Oct 2000 by Philip Homburg <philip@f-mnx.phicoh.com>

Provide 256-bit random numbers
*/

#define RAND256_BUFSIZE	32

void init_rand256 ARGS(( u8_t bits[RAND256_BUFSIZE] ));
void rand256 ARGS(( u8_t bits[RAND256_BUFSIZE] ));

/*
 * $PchId: rand256.h,v 1.1 2005/06/28 14:14:05 philip Exp $
 */
