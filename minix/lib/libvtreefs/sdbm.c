/* VTreeFS - sdbm.c - by Alen Stojanov and David van Moolenbroek */

/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Aake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain. keep it that way.
 *
 * hashing routine
 */

#include "inc.h"
/*
 * polynomial conversion ignoring overflows
 * [this seems to work remarkably well, in fact better
 * than the ndbm hash function. Replace at your own risk]
 * use: 65599	nice.
 *      65587   even better.
 */
long sdbm_hash(char *str, int len)
{
	unsigned long n = 0;

	while (len--)
		n = *str++ + (n << 6) + (n << 16) - n;

	return n;
}
