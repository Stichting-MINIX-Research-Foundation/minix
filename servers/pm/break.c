
#include "pm.h"
#include "param.h"
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
  r = vm_brk(mp->mp_endpoint, m_in.PMBRK_ADDR);
  mp->mp_reply.reply_ptr = (r == OK ? m_in.PMBRK_ADDR : (char *) -1);
  return r;
}

