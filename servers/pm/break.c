/*
 * XXX OBSOLETE AS OF 3.3.0, REMOVE
 */
#include "pm.h"
#include "glo.h"
#include "mproc.h"

#include <minix/vm.h>

/*===========================================================================*
 *				do_brk	 				     *
 *===========================================================================*/
int do_brk()
{
  int r;
/* Entry point to brk(addr) system call.  */
  r = vm_brk(mp->mp_endpoint, m_in.m1_p1);
  mp->mp_reply.m2_p1 = (r == OK ? m_in.m1_p1 : (char *) -1);
  return r;
}

