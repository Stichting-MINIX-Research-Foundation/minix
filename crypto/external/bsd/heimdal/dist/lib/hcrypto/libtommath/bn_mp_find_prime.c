/*	$NetBSD: bn_mp_find_prime.c,v 1.2 2017/01/28 21:31:47 christos Exp $	*/

/* TomsFastMath, a fast ISO C bignum library.
 *
 * This project is public domain and free for all purposes.
 *
 * Love Hornquist Astrand <lha@h5l.org>
 */
#include <tommath.h>
#ifdef BN_MP_FIND_PRIME_C
int mp_find_prime(mp_int *a, int t)
{
  int res = MP_NO;

  /* valid value of t? */
  if (t <= 0 || t > PRIME_SIZE) {
    return MP_VAL;
  }

  if (mp_iseven(a))
    mp_add_d(a, 1, a);

  do {
    if (mp_prime_is_prime(a, t, &res) != 0) {
      res = MP_VAL;
      break;
    }

    if (res == MP_NO) {
      mp_add_d(a, 2, a);
      continue;
    }

  } while (res != MP_YES);

  return res;
}
#endif
