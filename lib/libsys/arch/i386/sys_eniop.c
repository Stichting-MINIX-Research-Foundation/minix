#include "syslib.h"

/*===========================================================================*
 *                               sys_enable_iop				     *    
 *===========================================================================*/
int sys_enable_iop(proc_ep)
endpoint_t proc_ep;			/* number of process to allow I/O */
{
    message m_iop;
    m_iop.m_lsys_krn_sys_iopenable.endpt = proc_ep;
    return _kernel_call(SYS_IOPENABLE, &m_iop);
}


