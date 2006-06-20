#include "syslib.h"

/*===========================================================================*
 *                                sys_vinb				     *
 *===========================================================================*/
PUBLIC int sys_vinb(pvb_pairs, nr_ports)
pvb_pair_t *pvb_pairs;			/* (port,byte-value)-pairs */
int nr_ports;				/* nr of pairs to be processed */
{
    message m_io;

    m_io.DIO_REQUEST = _DIO_INPUT | _DIO_BYTE;
    m_io.DIO_VEC_ADDR = (char *) pvb_pairs;
    m_io.DIO_VEC_SIZE = nr_ports;
    return _taskcall(SYSTASK, SYS_VDEVIO, &m_io);
}

