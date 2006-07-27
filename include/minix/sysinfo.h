
#ifndef _MINIX_SYSINFO_H
#define _MINIX_SYSINFO_H

#include <minix/endpoint.h>
#include <minix/type.h>

_PROTOTYPE( int getsysinfo, (endpoint_t who, int what, void *where)       );
_PROTOTYPE( ssize_t getsysinfo_up, (endpoint_t who, int what, size_t size,
	void *where));

#define SIU_LOADINFO	1	/* retrieve load info data */

/* Exported system parameters. */

#endif

