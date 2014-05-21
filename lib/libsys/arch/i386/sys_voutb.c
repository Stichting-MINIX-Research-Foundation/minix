#include "syslib.h"

/*===========================================================================*
 *                                sys_voutb				     *
 *===========================================================================*/
int sys_voutb(pvb_pairs, nr_ports)
pvb_pair_t *pvb_pairs;			/* (port,byte-value)-pairs */
int nr_ports;				/* nr of pairs to be processed */
{
    message m_io;

    m_io.m_lsys_krn_sys_vdevio.request = _DIO_OUTPUT | _DIO_BYTE;
    m_io.m_lsys_krn_sys_vdevio.vec_addr = (vir_bytes)pvb_pairs;
    m_io.m_lsys_krn_sys_vdevio.vec_size = nr_ports;
    return _kernel_call(SYS_VDEVIO, &m_io);
}


