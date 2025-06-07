/* The kernel call implemented in this file:
 *   m_type:	SYS_DIAGCTL
 *
 * The parameters for this kernel call are:
 * 	m_lsys_krn_sys_diagctl.code	request
 * and then request-specific arguments in
 *	m_lsys_krn_sys_diagctl.buf
 *	m_lsys_krn_sys_diagctl.len
 *	m_lsys_krn_sys_diagctl.endpt
 */

#include "kernel/system.h"

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t or similar if error codes are mapped, and k_sigset_t for SIGKMESS
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *			        do_diagctl				     *
 *===========================================================================*/
int do_diagctl(struct proc * caller, message * m_ptr)
{
  vir_bytes len, buf;
  static char mybuf[DIAG_BUFSIZE];
  int s, i, proc_nr;

  switch (m_ptr->m_lsys_krn_sys_diagctl.code) {
    case DIAGCTL_CODE_DIAG:
        buf = m_ptr->m_lsys_krn_sys_diagctl.buf;
        len = m_ptr->m_lsys_krn_sys_diagctl.len;
	if(len < 1 || len > DIAG_BUFSIZE) {
		kprintf_stub("do_diagctl: diag for %d: len %d out of range\n", // MODIFIED
			caller->p_endpoint, len);
		return EINVAL; // EINVAL might be undefined
	}
	if((s=data_copy_vmcheck(caller, caller->p_endpoint, buf, KERNEL,
					(vir_bytes) mybuf, len)) != OK) {
		kprintf_stub("do_diagctl: diag for %d: len %d: copy failed: %d\n", // MODIFIED
			caller->p_endpoint, len, s);
		return s;
	}
	for(i = 0; i < len; i++)
		kputc(mybuf[i]);
	kputc(END_OF_KMESS);
	return OK;
    case DIAGCTL_CODE_STACKTRACE:
	if(!isokendpt(m_ptr->m_lsys_krn_sys_diagctl.endpt, &proc_nr))
		return EINVAL; // EINVAL might be undefined
	proc_stacktrace(proc_addr(proc_nr));
	return OK;
    case DIAGCTL_CODE_REGISTER:
	if (!(priv(caller)->s_flags & SYS_PROC))
		return EPERM; // EPERM might be undefined
	priv(caller)->s_diag_sig = TRUE;
	/* If the message log is not empty, send a first notification
	 * immediately. After bootup the log is basically never empty.
	 */
	if (kmess.km_size > 0 && !kinfo.do_serial_debug)
		send_sig(caller->p_endpoint, SIGKMESS); // SIGKMESS might be undefined
	return OK;
    case DIAGCTL_CODE_UNREGISTER:
	if (!(priv(caller)->s_flags & SYS_PROC))
		return EPERM; // EPERM might be undefined
	priv(caller)->s_diag_sig = FALSE;
	return OK;
    default:
	kprintf_stub("do_diagctl: invalid request %d\n", m_ptr->m_lsys_krn_sys_diagctl.code); // MODIFIED
        return(EINVAL); // EINVAL might be undefined
  }
}
