/* The kernel call implemented in this file:
 *   m_type:	SYS_PARAMCTL
 *
 * The parameters for this kernel call are:
 *      PCTL_REQ	request code (SYS_PARAM_*)
 *      PCTL_INT[12]	integer parameters
 *	PCTL_ADDR1	address parameter
 */

#include "../system.h"
#include <minix/safecopies.h>

/*===========================================================================*
 *				do_paramctl				     *
 *===========================================================================*/
PUBLIC int do_paramctl(m_ptr)
message *m_ptr;
{
	struct proc *rp;
	int r;

	/* Who wants to set a parameter? */
	rp = proc_addr(who_p);

	/* Which parameter is it? */
	switch(m_ptr->PCTL_REQ) {

		case SYS_PARAM_SET_GRANT:
			/* Copy grant table set in priv. struct. */
			if ((rp->p_rts_flags & NO_PRIV) || !(priv(rp))) {
				r = EPERM;
			} else {
				_K_SET_GRANT_TABLE(rp, 
					(vir_bytes) m_ptr->PCTL_ADDR1,
					m_ptr->PCTL_INT1);
				r = OK;
			}
			break;
		default:
			r = EINVAL;
			break;
	}

	return r;
}
