/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 *
 * Module:	range checks for LONGINT
 * Author:	Ceriel J.H. Jacobs
 * Version:	$Header$
*/

#include <em_abs.h>

extern TRP();

struct range_descr {
  long	low, high;
};

rckil(descr, val)
  struct range_descr *descr;
  long val;
{
  if (val < descr->low || val > descr->high) TRP(ERANGE);
}
