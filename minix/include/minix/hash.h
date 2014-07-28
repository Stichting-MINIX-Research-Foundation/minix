
#ifndef _MINIX_HASH_H
#define _MINIX_HASH_H 1

#include <stdint.h>

/* This code is taken from:
 * lookup3.c, by Bob Jenkins, May 2006, Public Domain.
 * (macro names modified)
 */

#define hash_rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define hash_mix(a,b,c) \
{ \
  a -= c;  a ^= hash_rot(c, 4);  c += b; \
  b -= a;  b ^= hash_rot(a, 6);  a += c; \
  c -= b;  c ^= hash_rot(b, 8);  b += a; \
  a -= c;  a ^= hash_rot(c,16);  c += b; \
  b -= a;  b ^= hash_rot(a,19);  a += c; \
  c -= b;  c ^= hash_rot(b, 4);  b += a; \
}

#define hash_final(a,b,c) \
{ \
  c ^= b; c -= hash_rot(b,14); \
  a ^= c; a -= hash_rot(c,11); \
  b ^= a; b -= hash_rot(a,25); \
  c ^= b; c -= hash_rot(b,16); \
  a ^= c; a -= hash_rot(c,4);  \
  b ^= a; b -= hash_rot(a,14); \
  c ^= b; c -= hash_rot(b,24); \
}

#define hash_i_64(a, u, v) {				\
	u32_t i1 = (a), i2 = ex64lo(u), i3 = ex64hi(u);	\
	hash_mix(i1, i2, i3);				\
	hash_final(i1, i2, i3);				\
	(v) = i3;					\
}

#define hash_32(n, v) {					\
	u32_t i1 = 0xa5a5a5a5, i2 = 0x12345678, i3 = n;	\
	hash_mix(i1, i2, i3);				\
	hash_final(i1, i2, i3);				\
	(v) = i3;					\
}

#endif
