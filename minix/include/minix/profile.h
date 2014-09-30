#ifndef _PROFILE_H
#define _PROFILE_H

#include <minix/type.h>
#include <sys/types.h>

/*
 * Types relating to system profiling.
 */

#  define PROF_START       0    /* start statistical profiling */
#  define PROF_STOP        1    /* stop statistical profiling */

#define PROF_RTC	0 /* RTC based profiling */
#define PROF_NMI	1 /* NMI based profiling, profiles kernel too */

/* Info struct to be copied to from kernel to user program. */
struct sprof_info_s {
  int mem_used;
  int total_samples;
  int idle_samples;
  int system_samples;
  int user_samples;
};

/* What a profiling sample looks like (used for sizeof()). */
struct sprof_sample {
	endpoint_t	proc;
	void *		pc;
};

struct sprof_proc {
	endpoint_t	proc;
	char		name[PROC_NAME_LEN];
};

int sprofile(int action, int size, int freq, int type, void *ctl_ptr,
	void *mem_ptr);

#endif /* PROFILE_H */
