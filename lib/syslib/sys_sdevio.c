#include "syslib.h"

/*===========================================================================*
 *                                sys_sdevio				     *
 *===========================================================================*/
PUBLIC int sys_sdevio(req, port, type, proc_nr, buffer, count)
int req;				/* request: DIO_INPUT/ DIO_OUTPUT */
long port; 				/* port address to read from */
int type;				/* byte, word, long */
int proc_nr;				/* process where buffer is */
void *buffer;				/* pointer to buffer */
int count;				/* number of elements */
{
    message m_io;
    int result;

    m_io.DIO_REQUEST = req;
    m_io.DIO_TYPE = type;
    m_io.DIO_PORT = port;
    m_io.DIO_VEC_PROC = proc_nr;
    m_io.DIO_VEC_ADDR = buffer;
    m_io.DIO_VEC_SIZE = count;

    return(_taskcall(SYSTASK, SYS_SDEVIO, &m_io));
}

