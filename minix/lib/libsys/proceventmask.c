#include "syslib.h"

#include <string.h>

/*
 * Subscribe to a certain set of process events from PM.  Subsequent calls will
 * replace the set, and the empty set (a zero mask) will unsubscribe the caller
 * altogether.  Usage restrictions apply; see PM's event.c for details.  Return
 * OK or a negative error code.
 */
int
proceventmask(unsigned int mask)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lsys_pm_proceventmask.mask = mask;

	return _taskcall(PM_PROC_NR, PM_PROCEVENTMASK, &m);
}
