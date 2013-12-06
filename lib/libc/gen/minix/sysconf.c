/* sysconf.c						POSIX 4.8.1
 *	long int sysconf(int name);
 *
 *	POSIX allows some of the values in <limits.h> to be increased at
 *	run time.  The sysconf() function allows such values to be checked
 *	at run time.  MINIX does not use this facility - the run time
 *	limits are those given in <limits.h>.
 */
#include <sys/cdefs.h>
#include "namespace.h"

#include <lib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <minix/paths.h>

#ifdef __weak_alias
__weak_alias(sysconf, __sysconf)
#endif

static u32_t get_hz(void)
{
  FILE *fp;
  u32_t hz;
  int r;

  if ((fp = fopen(_PATH_PROC "hz", "r")) != NULL)
  {
	r = fscanf(fp, "%u", &hz);

	fclose(fp);

	if (r == 1)
		return hz;
  }

  return DEFAULT_HZ;
}

long int sysconf(name)
int name;			/* property being inspected */
{
  switch(name) {
	case _SC_ARG_MAX:
		return (long) ARG_MAX;

	case _SC_CHILD_MAX:
		return (long) CHILD_MAX;

	case _SC_CLK_TCK:
		return (long) get_hz();

	case _SC_NGROUPS_MAX:
		return (long) NGROUPS_MAX;

	case _SC_OPEN_MAX:
		return (long) OPEN_MAX;

	case _SC_JOB_CONTROL:
		return -1L;			/* no job control */

	case _SC_SAVED_IDS:
		return -1L;			/* no saved uid/gid */

	case _SC_VERSION:
		return (long) _POSIX_VERSION;

	case _SC_STREAM_MAX:
		return (long) _POSIX_STREAM_MAX;

	case _SC_TZNAME_MAX:
		return (long) _POSIX_TZNAME_MAX;

	case _SC_PAGESIZE:
		return getpagesize();

	case _SC_LINE_MAX:
		return (long) LINE_MAX;

	default:
		errno = EINVAL;
		return -1L;
  }
}
