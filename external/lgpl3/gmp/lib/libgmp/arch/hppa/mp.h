/* mp-h.in -- Definitions for the GNU multiple precision library  -*-mode:c-*-
   BSD mp compatible functions.

Copyright 1991, 1993, 1994, 1995, 1996, 2000, 2001, 2002, 2004 Free Software
Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MP Library.  If not, see http://www.gnu.org/licenses/.  */

#ifndef __MP_H__


/* The following (everything under ifndef __GNU_MP__) must be identical in
   gmp.h and mp.h to allow both to be included in an application or during
   the library build.  Use the t-gmp-mp-h.pl script to check.  */
#ifndef __GNU_MP__
#define __GNU_MP__ 5

#define __need_size_t  /* tell gcc stddef.h we only want size_t */
#if defined (__cplusplus)
#include <cstddef>     /* for size_t */
#else
#include <stddef.h>    /* for size_t */
#endif
#undef __need_size_t

/* The following instantiated by configure, for internal use only */
#if ! defined (__GMP_WITHIN_CONFIGURE)
/* #undef _LONG_LONG_LIMB */
#define __GMP_LIBGMP_DLL  0
#endif

#if  defined (__STDC__)                                 \
  || defined (__cplusplus)                              \
  || defined (_AIX)                                     \
  || defined (__DECC)                                   \
  || (defined (__mips) && defined (_SYSTYPE_SVR4))      \
  || defined (_MSC_VER)                                 \
  || defined (_WIN32)
#define __GMP_HAVE_CONST        1
#define __GMP_HAVE_PROTOTYPES   1
#define __GMP_HAVE_TOKEN_PASTE  1
#else
#define __GMP_HAVE_CONST        0
#define __GMP_HAVE_PROTOTYPES   0
#define __GMP_HAVE_TOKEN_PASTE  0
#endif


#if __GMP_HAVE_CONST
#define __gmp_const   const
#define __gmp_signed  signed
#else
#define __gmp_const
#define __gmp_signed
#endif

#if defined (__GNUC__)
#define __GMP_DECLSPEC_EXPORT  __declspec(__dllexport__)
#define __GMP_DECLSPEC_IMPORT  __declspec(__dllimport__)
#endif
#if defined (_MSC_VER) || defined (__BORLANDC__)
#define __GMP_DECLSPEC_EXPORT  __declspec(dllexport)
#define __GMP_DECLSPEC_IMPORT  __declspec(dllimport)
#endif
#ifdef __WATCOMC__
#define __GMP_DECLSPEC_EXPORT  __export
#define __GMP_DECLSPEC_IMPORT  __import
#endif
#ifdef __IBMC__
#define __GMP_DECLSPEC_EXPORT  _Export
#define __GMP_DECLSPEC_IMPORT  _Import
#endif

#if __GMP_LIBGMP_DLL
#if __GMP_WITHIN_GMP
#define __GMP_DECLSPEC  __GMP_DECLSPEC_EXPORT
#else
#define __GMP_DECLSPEC  __GMP_DECLSPEC_IMPORT
#endif
#else
#define __GMP_DECLSPEC
#endif

#ifdef __GMP_SHORT_LIMB
typedef unsigned int		mp_limb_t;
typedef int			mp_limb_signed_t;
#else
#ifdef _LONG_LONG_LIMB
typedef unsigned long long int	mp_limb_t;
typedef long long int		mp_limb_signed_t;
#else
typedef unsigned long int	mp_limb_t;
typedef long int		mp_limb_signed_t;
#endif
#endif
typedef unsigned long int	mp_bitcnt_t;

typedef struct
{
  int _mp_alloc;		/* Number of *limbs* allocated and pointed
				   to by the _mp_d field.  */
  int _mp_size;			/* abs(_mp_size) is the number of limbs the
				   last field points to.  If _mp_size is
				   negative this is a negative number.  */
  mp_limb_t *_mp_d;		/* Pointer to the limbs.  */
} __mpz_struct;

#endif /* __GNU_MP__ */

/* User-visible types.  */
typedef __mpz_struct MINT;


#if __GMP_HAVE_PROTOTYPES
#define __GMP_PROTO(x) x
#else
#define __GMP_PROTO(x) ()
#endif

#if defined (__cplusplus)
extern "C" {
#endif

#define mp_set_memory_functions __gmp_set_memory_functions
__GMP_DECLSPEC void mp_set_memory_functions __GMP_PROTO ((void *(*) (size_t),
                                      void *(*) (void *, size_t, size_t),
                                      void (*) (void *, size_t)));
__GMP_DECLSPEC MINT *itom __GMP_PROTO ((signed short int));
__GMP_DECLSPEC MINT *xtom __GMP_PROTO ((const char *));
__GMP_DECLSPEC void move __GMP_PROTO ((const MINT *, MINT *));
__GMP_DECLSPEC void madd __GMP_PROTO ((const MINT *, const MINT *, MINT *));
__GMP_DECLSPEC void msub __GMP_PROTO ((const MINT *, const MINT *, MINT *));
__GMP_DECLSPEC void mult __GMP_PROTO ((const MINT *, const MINT *, MINT *));
__GMP_DECLSPEC void mdiv __GMP_PROTO ((const MINT *, const MINT *, MINT *, MINT *));
__GMP_DECLSPEC void sdiv __GMP_PROTO ((const MINT *, signed short int, MINT *, signed short int *));
__GMP_DECLSPEC void msqrt __GMP_PROTO ((const MINT *, MINT *, MINT *));
__GMP_DECLSPEC void pow __GMP_PROTO ((const MINT *, const MINT *, const MINT *, MINT *));
__GMP_DECLSPEC void rpow __GMP_PROTO ((const MINT *, signed short int, MINT *));
__GMP_DECLSPEC void gcd __GMP_PROTO ((const MINT *, const MINT *, MINT *));
__GMP_DECLSPEC int  mcmp __GMP_PROTO ((const MINT *, const MINT *));
__GMP_DECLSPEC void min __GMP_PROTO ((MINT *));
__GMP_DECLSPEC void mout __GMP_PROTO ((const MINT *));
__GMP_DECLSPEC char *mtox __GMP_PROTO ((const MINT *));
__GMP_DECLSPEC void mfree __GMP_PROTO ((MINT *));

#if defined (__cplusplus)
}
#endif

#define __MP_H__
#endif /* __MP_H__ */
