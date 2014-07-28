#include "syslib.h"

/*===========================================================================*
 *                                sys_sdevio				     *
 *===========================================================================*/
int sys_sdevio(req, port, proc_nr, buffer, count, offset)
int req;				/* request: DIO_{IN,OUT}PUT_* */
long port; 				/* port address to read from */
endpoint_t proc_nr;			/* process where buffer is */
void *buffer;				/* pointer to buffer */
int count;				/* number of elements */
vir_bytes offset;			/* offset from grant */
{
    message m_io;

    m_io.m_lsys_krn_sys_sdevio.request = req;
    m_io.m_lsys_krn_sys_sdevio.port = port;
    m_io.m_lsys_krn_sys_sdevio.vec_endpt = proc_nr;
    m_io.m_lsys_krn_sys_sdevio.vec_addr = buffer;
    m_io.m_lsys_krn_sys_sdevio.vec_size = count;
    m_io.m_lsys_krn_sys_sdevio.offset = offset;

    return(_kernel_call(SYS_SDEVIO, &m_io));
}

