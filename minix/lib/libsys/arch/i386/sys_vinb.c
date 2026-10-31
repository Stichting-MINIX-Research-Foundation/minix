#include "syslib.h"

/*===========================================================================*
 *                                sys_vinb				     *
 *===========================================================================*/
int sys_vinb(pvb_pair_t *pvb_pairs, int nr_ports)
{
    message m_io;

    m_io.m_lsys_krn_sys_vdevio.request = _DIO_INPUT | _DIO_BYTE;
    m_io.m_lsys_krn_sys_vdevio.vec_addr = (vir_bytes) pvb_pairs;
    m_io.m_lsys_krn_sys_vdevio.vec_size = nr_ports;
    return _kernel_call(SYS_VDEVIO, &m_io);
}

