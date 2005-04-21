/*
 * (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 *
 * Module:	range checks for INTEGER, now for array indexing
 * Author:	Ceriel J.H. Jacobs
 * Version:	$Header$
*/

#include <em_abs.h>

extern TRP();

struct array_descr {
  int	lbound;
  int	n_elts_min_one;
  unsigned size;
};

rcka(descr, indx)
  struct array_descr *descr;
{
  if (indx < 0 || indx > descr->n_elts_min_one) TRP(EARRAY);
}
