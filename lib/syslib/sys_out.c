#include "syslib.h"

/*===========================================================================*
 *                                sys_out				     *
 *===========================================================================*/
PUBLIC int sys_out(port, value, type)
int port; 				/* port address to write to */
unsigned long value;			/* value to write */
int type;				/* byte, word, long */
{
    message m_io;

    m_io.DIO_TYPE = type;
    m_io.DIO_REQUEST = DIO_OUTPUT;
    m_io.DIO_PORT = port;
    m_io.DIO_VALUE = value;

    return _taskcall(SYSTASK, SYS_DEVIO, &m_io);
}

