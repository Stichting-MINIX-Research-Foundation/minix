#include "syslib.h"

/*===========================================================================*
 *                                sys_in				     *
 *===========================================================================*/
int sys_in(port, value, type)
int port; 				/* port address to read from */
u32_t *value;				/* pointer where to store value */
int type;				/* byte, word, long */
{
    message m_io;
    int result;

    m_io.m_lsys_krn_sys_devio.request = _DIO_INPUT | type;
    m_io.m_lsys_krn_sys_devio.port = port;

    result = _kernel_call(SYS_DEVIO, &m_io);
    *value = m_io.m_krn_lsys_sys_devio.value;
    return(result);
}

