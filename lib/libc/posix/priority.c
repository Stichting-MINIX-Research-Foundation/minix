/*
priority.c
*/

#include <errno.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <lib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>


int getpriority(int which, int who)
{
	int v;
	message m;

	m.m1_i1 = which;
	m.m1_i2 = who;

	/* GETPRIORITY returns negative for error.
	 * Otherwise, it returns the priority plus the minimum
	 * priority, to distiginuish from error. We have to
	 * correct for this. (The user program has to check errno
	 * to see if something really went wrong.)
	 */

	if((v = _syscall(MM, GETPRIORITY, &m)) < 0) {
		return v;
	}

	return v + PRIO_MIN;
}

int setpriority(int which, int who, int prio)
{
	message m;

	m.m1_i1 = which;
	m.m1_i2 = who;
	m.m1_i3 = prio;

	return _syscall(MM, SETPRIORITY, &m);
}

