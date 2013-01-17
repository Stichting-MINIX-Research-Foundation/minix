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

    m_io.DIO_REQUEST = _DIO_INPUT | type;
    m_io.DIO_PORT = port;

    result = _kernel_call(SYS_DEVIO, &m_io);
    *value = m_io.DIO_VALUE;
    return(result);
}

