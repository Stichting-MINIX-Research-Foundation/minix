#include "../syslib/syslib.h"

/*===========================================================================*
 *                               sys_enable_iop				     *    
 *===========================================================================*/
PUBLIC int sys_enable_iop(proc_nr)
int proc_nr;			/* number of process to allow I/O */
{
    message m_iop;
    m_iop.IO_ENDPT = proc_nr;
    return _taskcall(SYSTASK, SYS_IOPENABLE, &m_iop);
}


