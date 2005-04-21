/*  ctermid(3)
 *
 *  Author: Terrence Holm          Aug. 1988
 *
 *
 *  Ctermid(3) returns a pointer to a string naming the controlling
 *  terminal. If <name_space> is NULL then local PRIVATE storage
 *  is used, otherwise <name_space> must point to storage of at
 *  least L_ctermid characters.
 *
 *  Returns a pointer to "/dev/tty".
 */

#include <lib.h>
#include <string.h>
#include <stdio.h>

_PROTOTYPE( char *ctermid, (char *name_space));

#ifndef L_ctermid
#define L_ctermid  9
#endif

char *ctermid(name_space)
char *name_space;
{
  PRIVATE char termid[L_ctermid];

  if (name_space == (char *)NULL) name_space = termid;
  strcpy(name_space, "/dev/tty");
  return(name_space);
}
