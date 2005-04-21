#include "syslib.h"

/*===========================================================================*
 *                                sys_in				     *
 *===========================================================================*/
PUBLIC int sys_in(port, value, type)
int port; 				/* port address to read from */
unsigned long *value;			/* pointer where to store value */
int type;				/* byte, word, long */
{
    message m_io;
    int result;

    m_io.DIO_TYPE = type;
    m_io.DIO_REQUEST = DIO_INPUT;
    m_io.DIO_PORT = port;

    result = _taskcall(SYSTASK, SYS_DEVIO, &m_io);
    *value = m_io.DIO_VALUE;
    return(result);
}

