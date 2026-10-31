#include "syslib.h"

/*===========================================================================*
 *                                sys_sdevio				     *
 *===========================================================================*/
int sys_sdevio(int req, long port, endpoint_t proc_nr, void *buffer, int count, vir_bytes offset)
{
    message m_io;

    m_io.m_lsys_krn_sys_sdevio.request = req;
    m_io.m_lsys_krn_sys_sdevio.port = port;
    m_io.m_lsys_krn_sys_sdevio.vec_endpt = proc_nr;
    m_io.m_lsys_krn_sys_sdevio.vec_addr = (vir_bytes)buffer;
    m_io.m_lsys_krn_sys_sdevio.vec_size = count;
    m_io.m_lsys_krn_sys_sdevio.offset = offset;

    return(_kernel_call(SYS_SDEVIO, &m_io));
}

