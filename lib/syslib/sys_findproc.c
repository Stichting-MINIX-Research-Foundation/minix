#include "syslib.h"
#include <string.h>

int sys_findproc(name, tasknr, flags)
char *name;
int *tasknr;
int flags;
{
/* Map a task name to a task number. */
	message m;
	int r;

	strncpy(m.m3_ca1, name, M3_STRING);
	m.m3_i1= flags;

	/* Clear unused fields */
	m.m3_i2 = 0;
	m.m3_p1= NULL;

	r= _taskcall(SYSTASK, SYS_FINDPROC, &m);

	*tasknr= m.m3_i1;
	return r;
}

/*
 * $PchId: sys_findproc.c,v 1.2 1996/04/11 05:46:27 philip Exp $
 */
