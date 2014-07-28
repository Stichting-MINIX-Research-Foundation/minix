/*
priority.c
*/

#include <sys/cdefs.h>
#include "namespace.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <lib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>


int getpriority(int which, id_t who)
{
	int v;
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_pm_priority.which = which;
	m.m_lc_pm_priority.who = who;

	/* GETPRIORITY returns negative for error.
	 * Otherwise, it returns the priority plus the minimum
	 * priority, to distiginuish from error. We have to
	 * correct for this. (The user program has to check errno
	 * to see if something really went wrong.)
	 */

	if((v = _syscall(PM_PROC_NR, PM_GETPRIORITY, &m)) < 0) {
		return v;
	}

	return v + PRIO_MIN;
}

int setpriority(int which, id_t who, int prio)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.m_lc_pm_priority.which = which;
	m.m_lc_pm_priority.who = who;
	m.m_lc_pm_priority.prio = prio;

	return _syscall(PM_PROC_NR, PM_SETPRIORITY, &m);
}

