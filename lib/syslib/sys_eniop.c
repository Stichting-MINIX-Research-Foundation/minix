#include "syslib.h"

/*===========================================================================*
 *                               sys_enable_iop				     *    
 *===========================================================================*/
PUBLIC int sys_enable_iop(proc_nr_e)
int proc_nr_e;			/* number of process to allow I/O */
{
    message m_iop;
    m_iop.IO_ENDPT = proc_nr_e;
    return _taskcall(SYSTASK, SYS_IOPENABLE, &m_iop);
}


