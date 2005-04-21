#include "syslib.h"

/*===========================================================================*
 *                                sys_voutl				     *
 *===========================================================================*/
PUBLIC int sys_voutl(pvl_pairs, nr_ports)
pvl_pair_t *pvl_pairs;			/* (port,long-value)-pairs */
int nr_ports;				/* nr of pairs to be processed */
{
    message m_io;

    m_io.DIO_TYPE = DIO_LONG;
    m_io.DIO_REQUEST = DIO_OUTPUT;
    m_io.DIO_VEC_ADDR = (char *) pvl_pairs;
    m_io.DIO_VEC_SIZE = nr_ports;
    return _taskcall(SYSTASK, SYS_VDEVIO, &m_io);
}

