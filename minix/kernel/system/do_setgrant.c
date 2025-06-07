/* The kernel call implemented in this file:
 *   m_type:	SYS_SETGRANT
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_setgrant.addr    address of grant table in own address space
 *   m_lsys_krn_sys_setgrant.size    number of entries
 */

#include "kernel/system.h"
#include <minix/safecopies.h> // Kept

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t or similar if error codes are mapped
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *				do_setgrant				     *
 *===========================================================================*/
int do_setgrant(struct proc * caller, message * m_ptr)
{
	int r;

	/* Copy grant table set in priv. struct. */
	if (RTS_ISSET(caller, RTS_NO_PRIV) || !(priv(caller))) {
		r = EPERM; // EPERM might be undefined
	} else {
		_K_SET_GRANT_TABLE(caller,
			m_ptr->m_lsys_krn_sys_setgrant.addr,
			m_ptr->m_lsys_krn_sys_setgrant.size);
		r = OK;
	}

	return r;
}
