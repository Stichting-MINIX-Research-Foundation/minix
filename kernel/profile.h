#ifndef PROFILE_H
#define PROFILE_H

#if SPROFILE || CPROFILE
#include <minix/profile.h>
#endif

#if SPROFILE	/* statistical profiling */

EXTERN int sprofiling;			/* whether profiling is running */
EXTERN int sprof_mem_size;		/* available user memory for data */
EXTERN struct sprof_info_s sprof_info;	/* profiling info for user program */
EXTERN phys_bytes sprof_data_addr;	/* user address to write data */
EXTERN phys_bytes sprof_info_addr;	/* user address to write info struct */

#endif /* SPROFILE */


#if CPROFILE	/* call profiling */

EXTERN int cprof_mem_size;		/* available user memory for data */
EXTERN struct cprof_info_s cprof_info;	/* profiling info for user program */
EXTERN phys_bytes cprof_data_addr;	/* user address to write data */
EXTERN phys_bytes cprof_info_addr;	/* user address to write info struct */
EXTERN int cprof_procs_no;		/* number of profiled processes */
EXTERN struct cprof_proc_info_s {	/* info about profiled process */
	int endpt;			/* endpoint */
	char *name;			/* name */
	phys_bytes ctl;			/* location of control struct */
	phys_bytes buf;			/* location of buffer */
	int slots_used;			/* table slots used */
} cprof_proc_info_inst;
EXTERN struct cprof_proc_info_s cprof_proc_info[NR_SYS_PROCS];	

#endif /* CPROFILE */

#endif /* PROFILE_H */

