
#include <lib.h>
#include <unistd.h>
#include <timers.h>
#include <minix/endpoint.h>
#define getsysinfo	_getsysinfo
#define getsysinfo_up	_getsysinfo_up
#include <minix/sysinfo.h>
#include <minix/const.h>

#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/sysinfo.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/const.h>
#include <stdio.h>
#include <stdlib.h>

#include <machine/archtypes.h>
#include "../../../kernel/proc.h"

PUBLIC int getsysinfo(who, what, where)
endpoint_t who;			/* from whom to request info */
int what;			/* what information is requested */
void *where;			/* where to put it */
{
  message m;
  m.m1_i1 = what;
  m.m1_p1 = where;
  if (_syscall(who, GETSYSINFO, &m) < 0) return(-1);
  return(0);
}

PUBLIC int minix_getkproctab(void *vpr, int nprocs, int assert)
{
	int r, i, fail = 0;
	struct proc *pr = vpr;

	if((r=getsysinfo(PM_PROC_NR, SI_KPROC_TAB, pr)) < 0)
		return r;

	for(i = 0; i < nprocs; i++) {
		if(pr[i].p_magic != PMAGIC) {
			fail = 1;
			break;
		}
	}

	if(!fail)
		return 0;

	if(assert) {
		fprintf(stderr, "%s: process table failed sanity check.\n", getprogname());
		fprintf(stderr, "is kernel out of sync?\n");
		exit(1);
	}

	errno = ENOSYS;

	return -1;
}

/* Unprivileged variant of getsysinfo. */
PUBLIC ssize_t getsysinfo_up(who, what, size, where)
endpoint_t who;			/* from whom to request info */
int what;			/* what information is requested */
size_t size;			/* input and output size */
void *where;			/* where to put it */
{
  message m;
  m.SIU_WHAT = what;
  m.SIU_WHERE = where;
  m.SIU_LEN = size;
  return _syscall(who, GETSYSINFO_UP, &m);
}

