/*	sysuname(2) - transfer uname(3) strings.	Author: Kees J. Bot
 *								5 Dec 1992
 */

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <string.h>
#include <sys/utsname.h>

int sysuname(int req, int field, char *value, size_t len)
{
	message m;

	memset(&m, 0, sizeof(m));
	m.PM_SYSUNAME_REQ = req;
	m.PM_SYSUNAME_FIELD = field;
	m.PM_SYSUNAME_LEN = len;
	m.PM_SYSUNAME_VALUE = value;

	return _syscall(PM_PROC_NR, PM_SYSUNAME, &m);
}

/*
 * $PchId: _sysuname.c,v 1.4 1995/11/27 19:42:09 philip Exp $
 */
