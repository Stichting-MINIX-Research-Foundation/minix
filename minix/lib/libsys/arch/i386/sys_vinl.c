#include "syslib.h"

/*===========================================================================*
 *                                sys_vinl				     *
 *===========================================================================*/
int sys_vinl(pvl_pair_t *pvl_pairs, int nr_ports)
{
    message m_io;

    m_io.m_lsys_krn_sys_vdevio.request = _DIO_INPUT | _DIO_LONG;
    m_io.m_lsys_krn_sys_vdevio.vec_addr = (vir_bytes)pvl_pairs;
    m_io.m_lsys_krn_sys_vdevio.vec_size = nr_ports;
    return _kernel_call(SYS_VDEVIO, &m_io);
}

