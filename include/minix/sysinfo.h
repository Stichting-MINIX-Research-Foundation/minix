
#ifndef _MINIX_SYSINFO_H
#define _MINIX_SYSINFO_H

#include <minix/endpoint.h>
#include <minix/type.h>

int getsysinfo(endpoint_t who, int what, void *where, size_t size);

/* What system info to retrieve with sysgetinfo(). */
#define SI_PROC_TAB	   2	/* copy of entire process table */
#define SI_DMAP_TAB	   3	/* get device <-> driver mappings */
#define SI_DATA_STORE	   5	/* get copy of data store mappings */
#define SI_CALL_STATS	   9	/* system call statistics */
#define SI_PROCPUB_TAB	   11	/* copy of public entries of process table */
#define SI_VMNT_TAB        12   /* get vmnt table */

#endif

