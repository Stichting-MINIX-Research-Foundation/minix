/* VTreeFS - utility.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys(void)
{
	/* This call is not recognized by VTreeFS. If a message hook is
	 * defined, let it handle the call; otherwise return ENOSYS.
	 */

	if (vtreefs_hooks->message_hook != NULL)
		return vtreefs_hooks->message_hook(&fs_m_in);

	return ENOSYS;
}

/*===========================================================================*
 *				do_noop					     *
 *===========================================================================*/
int do_noop(void)
{
	/* This call has no effect.
	 */

	return OK;
}
