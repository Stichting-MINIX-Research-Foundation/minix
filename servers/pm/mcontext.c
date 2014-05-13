#include "pm.h"
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/vm.h>
#include "mproc.h"


/*===========================================================================*
 *				do_setmcontext				     *
 *===========================================================================*/
int do_setmcontext()
{
  return sys_setmcontext(who_e, m_in.m_lc_pm_mcontext.ctx);
}


/*===========================================================================*
 *				do_getmcontext				     *
 *===========================================================================*/
int do_getmcontext()
{
  return sys_getmcontext(who_e, m_in.m_lc_pm_mcontext.ctx);
}

