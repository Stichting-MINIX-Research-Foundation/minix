#include "syslib.h"

/*===========================================================================*
 *                                sys_vinl				     *
 *===========================================================================*/
int sys_vinl(pvl_pairs, nr_ports)
pvl_pair_t *pvl_pairs;			/* (port,long-value)-pairs */
int nr_ports;				/* nr of pairs to be processed */
{
    message m_io;

    m_io.DIO_REQUEST = _DIO_INPUT | _DIO_LONG;
    m_io.DIO_VEC_ADDR = (char *) pvl_pairs;
    m_io.DIO_VEC_SIZE = nr_ports;
    return _kernel_call(SYS_VDEVIO, &m_io);
}

