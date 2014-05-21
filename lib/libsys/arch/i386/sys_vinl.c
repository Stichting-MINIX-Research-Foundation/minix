#include "syslib.h"

/*===========================================================================*
 *                                sys_vinl				     *
 *===========================================================================*/
int sys_vinl(pvl_pairs, nr_ports)
pvl_pair_t *pvl_pairs;			/* (port,long-value)-pairs */
int nr_ports;				/* nr of pairs to be processed */
{
    message m_io;

    m_io.m_lsys_krn_sys_vdevio.request = _DIO_INPUT | _DIO_LONG;
    m_io.m_lsys_krn_sys_vdevio.vec_addr = (vir_bytes)pvl_pairs;
    m_io.m_lsys_krn_sys_vdevio.vec_size = nr_ports;
    return _kernel_call(SYS_VDEVIO, &m_io);
}

