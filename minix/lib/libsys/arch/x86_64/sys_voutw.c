#include "syslib.h"

/*===========================================================================*
 *                                sys_voutw				     *
 *===========================================================================*/
int sys_voutw(pvw_pair_t *pvw_pairs, int nr_ports)
{
    message m_io;

    m_io.m_lsys_krn_sys_vdevio.request = _DIO_OUTPUT | _DIO_WORD;
    m_io.m_lsys_krn_sys_vdevio.vec_addr = (vir_bytes)pvw_pairs;
    m_io.m_lsys_krn_sys_vdevio.vec_size = nr_ports;
    return _kernel_call(SYS_VDEVIO, &m_io);
}

