/*	sysuname(2) - transfer uname(3) strings.	Author: Kees J. Bot
 *								5 Dec 1992
 */

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <sys/utsname.h>

int sysuname(int req, int field, char *value, size_t len)
{
	message m;

	m.m1_i1 = req;
	m.m1_i2 = field;
	m.m1_i3 = len;
	m.m1_p1 = value;

	/* Clear unused fields */
	m.m1_p2 = NULL;
	m.m1_p3 = NULL;

	return _syscall(PM_PROC_NR, SYSUNAME, &m);
}

/*
 * $PchId: _sysuname.c,v 1.4 1995/11/27 19:42:09 philip Exp $
 */
