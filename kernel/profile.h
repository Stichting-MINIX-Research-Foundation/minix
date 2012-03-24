#ifndef PROFILE_H
#define PROFILE_H

#include <minix/profile.h>

#if SPROFILE	/* statistical profiling */

#include "arch_watchdog.h"

#define SAMPLE_BUFFER_SIZE	(64 << 20)
extern char sprof_sample_buffer[SAMPLE_BUFFER_SIZE];

EXTERN int sprofiling;			/* whether profiling is running */
EXTERN int sprofiling_type;			/* whether profiling is running */
EXTERN int sprof_mem_size;		/* available user memory for data */
EXTERN struct sprof_info_s sprof_info;	/* profiling info for user program */
EXTERN vir_bytes sprof_data_addr_vir;	/* user address to write data */
EXTERN endpoint_t sprof_ep;		/* user process */

void nmi_sprofile_handler(struct nmi_frame * frame);

#endif /* SPROFILE */


EXTERN int cprof_mem_size;		/* available user memory for data */
EXTERN struct cprof_info_s cprof_info;	/* profiling info for user program */
EXTERN int cprof_procs_no;		/* number of profiled processes */
EXTERN struct cprof_proc_info_s {	/* info about profiled process */
	endpoint_t endpt;		/* endpoint */
	char *name;			/* name */
	vir_bytes ctl_v;		/* location of control struct */
	vir_bytes buf_v;		/* location of buffer */
	int slots_used;			/* table slots used */
} cprof_proc_info_inst;
EXTERN struct cprof_proc_info_s cprof_proc_info[NR_SYS_PROCS];	

#endif /* PROFILE_H */

