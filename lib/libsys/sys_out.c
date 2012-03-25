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

    m_io.DIO_REQUEST = _DIO_OUTPUT | type;
    m_io.DIO_PORT = port;
    m_io.DIO_VALUE = value;

    return _kernel_call(SYS_DEVIO, &m_io);
}

