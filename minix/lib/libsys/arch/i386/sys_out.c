#include "syslib.h"

/*===========================================================================*
 *                                sys_out				     *
 *===========================================================================*/
int sys_out(port, value, type)
int port; 				/* port address to write to */
u32_t value;				/* value to write */
int type;				/* byte, word, long */
{
    message m_io;

    m_io.m_lsys_krn_sys_devio.request = _DIO_OUTPUT | type;
    m_io.m_lsys_krn_sys_devio.port = port;
    m_io.m_lsys_krn_sys_devio.value = value;

    return _kernel_call(SYS_DEVIO, &m_io);
}

