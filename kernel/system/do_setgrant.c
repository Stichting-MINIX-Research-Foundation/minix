/* The kernel call implemented in this file:
 *   m_type:	SYS_SETGRANT
 *
 * The parameters for this kernel call are:
 *      SG_ADDR	address of grant table in own address space
 *	SG_SIZE	number of entries
 */

#include "kernel/system.h"
#include <minix/safecopies.h>

/*===========================================================================*
 *				do_setgrant				     *
 *===========================================================================*/
int do_setgrant(struct proc * caller, message * m_ptr)
{
	int r;

	/* Copy grant table set in priv. struct. */
	if (RTS_ISSET(caller, RTS_NO_PRIV) || !(priv(caller))) {
		r = EPERM;
	} else {
		_K_SET_GRANT_TABLE(caller,
			(vir_bytes) m_ptr->SG_ADDR,
			m_ptr->SG_SIZE);
		r = OK;
	}

	return r;
}
