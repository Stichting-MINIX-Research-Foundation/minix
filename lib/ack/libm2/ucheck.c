/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 *
 * Module:	CARDINAL operations with overflow checking
 * Author:	Ceriel J.H. Jacobs
 * Version:	$Header$
*/

#ifndef EM_WSIZE
#define EM_WSIZE _EM_WSIZE
#endif
#ifndef EM_LSIZE
#define EM_LSIZE _EM_LSIZE
#endif

#include <m2_traps.h>

#define MAXCARD	((unsigned)-1)
#if EM_WSIZE < EM_LSIZE
#define MAXLONGCARD	((unsigned long) -1L)
#endif

adduchk(a,b)
  unsigned	a,b;
{
  if (MAXCARD - a < b) TRP(M2_UOVFL);
}

#if EM_WSIZE < EM_LSIZE
addulchk(a,b)
  unsigned long	a,b;
{
  if (MAXLONGCARD - a < b) TRP(M2_UOVFL);
}
#endif

muluchk(a,b)
  unsigned	a,b;
{
  if (a != 0 && MAXCARD/a < b) TRP(M2_UOVFL);
}

#if EM_WSIZE < EM_LSIZE
mululchk(a,b)
  unsigned long	a,b;
{
  if (a != 0 && MAXLONGCARD/a < b) TRP(M2_UOVFL);
}
#endif

subuchk(a,b)
  unsigned	a,b;
{
  if (b < a) TRP(M2_UUVFL);
}

#if EM_WSIZE < EM_LSIZE
subulchk(a,b)
  unsigned long	a,b;
{
  if (b < a) TRP(M2_UUVFL);
}
#endif
