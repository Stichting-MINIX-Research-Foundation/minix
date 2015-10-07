/*	$NetBSD: bn_mp_find_prime.c,v 1.1.1.1 2011/04/13 18:14:54 elric Exp $	*/

/* TomsFastMath, a fast ISO C bignum library.
 * 
 * This project is public domain and free for all purposes.
 * 
 * Love Hornquist Astrand <lha@h5l.org>
 */
#include <tommath.h>

int mp_find_prime(mp_int *a)
{
  int res;

  if (mp_iseven(a))
    mp_add_d(a, 1, a);

  do {

    if ((res = mp_isprime(a)) == MP_NO) {
      mp_add_d(a, 2, a);
      continue;
    }

  } while (res != MP_YES);

  return res;
}
