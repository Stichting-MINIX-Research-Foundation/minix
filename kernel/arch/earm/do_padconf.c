#include "kernel/system.h"
#include <minix/endpoint.h>

#if USE_PADCONF

/* get arch specific arch_padconf_set() */
#if defined(AM335X) || defined(DM37XX)
#include "omap_padconf.h"
#endif

/*===========================================================================*
 *                                do_padconf                                 *
 *===========================================================================*/
int do_padconf(struct proc *caller_ptr, message *m_ptr)
{
	return arch_padconf_set(m_ptr->PADCONF_PADCONF, m_ptr->PADCONF_MASK,
	    m_ptr->PADCONF_VALUE);
}

#endif /* USE_PADCONF */
