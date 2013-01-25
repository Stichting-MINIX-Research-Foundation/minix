#include "syslib.h"


/*===========================================================================*
 *                                sys_voutw				     *
 *===========================================================================*/
int sys_voutw(pvw_pairs, nr_ports)
pvw_pair_t *pvw_pairs;			/* (port,word-value)-pairs */
int nr_ports;				/* nr of pairs to be processed */
{
    message m_io;

    m_io.DIO_REQUEST = _DIO_OUTPUT | _DIO_WORD;
    m_io.DIO_VEC_ADDR = (char *) pvw_pairs;
    m_io.DIO_VEC_SIZE = nr_ports;
    return _kernel_call(SYS_VDEVIO, &m_io);
}

