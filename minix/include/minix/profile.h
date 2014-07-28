#ifndef _PROFILE_H
#define _PROFILE_H

#include <minix/type.h>
#include <sys/types.h>

/*
 * Types relating to system profiling.  Types are supplied for both
 * statistical profiling and call profiling.
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

#  define PROF_GET         2    /* get call profiling tables */
#  define PROF_RESET       3    /* reset call profiling tables */

/* Hash table size in each profiled process is table size + index size.
 *
 * Table size = CPROF_TABLE_SIZE * (CPROF_CPATH_MAX_LEN + 16).
 * Index size = CPROF_INDEX_SIZE * 4;
 *
 * Making CPROF_CPATH_MAX_LEN too small may cause call path overruns.
 * Making CPROF_TABLE_SIZE too small may cause table overruns.
 * 
 * There are some restrictions: processes in the boot image are loaded
 * below 16 MB and the kernel is loaded in lower memory (below 640 kB). The
 * latter is reason to use a different size for the kernel table.
 */
#define CPROF_TABLE_SIZE_OTHER	3000	/* nr of slots in hash table */
#define CPROF_TABLE_SIZE_KERNEL	1500	/* kernel has a smaller table */
#define CPROF_CPATH_MAX_LEN	256	/* len of cpath string field: */
					/* MUST BE MULTIPLE OF WORDSIZE */

#define CPROF_INDEX_SIZE	(10*1024)/* size of index to hash table */
#define CPROF_STACK_SIZE	24	/* size of call stack */
#define CPROF_PROCNAME_LEN	8	/* len of proc name field */

#define CPROF_CPATH_OVERRUN	0x1	/* call path overrun */
#define CPROF_STACK_OVERRUN	0x2	/* call stack overrun */
#define CPROF_TABLE_OVERRUN	0x4	/* hash table overrun */

#define CPROF_ANNOUNCE_OTHER	1	/* processes announce their profiling
					 * data on n-th entry of procentry */
#define CPROF_ACCOUNCE_KERNEL	10000	/* kernel announces not directly */

/* Prototype for function called by procentry to get size of table. */
int profile_get_tbl_size(void);
/* Prototype for function called by procentry to get announce number. */
int profile_get_announce(void);
/* Prototype for function called by procentry to announce control struct
 * and table locations to the kernel. */
void profile_register(void *ctl_ptr, void *tbl_ptr);

/* Info struct to be copied from kernel to user program. */
struct cprof_info_s {
  int mem_used;
  int err;
};

/* Data structures for control structure and profiling data table in the
 * in the profiled processes. 
 */
struct cprof_ctl_s {
  int reset;				/* kernel sets to have table reset */
  int slots_used;			/* proc writes nr slots used in table */
  int err;				/* proc writes errors that occurred */
};

struct cprof_tbl_s {
  struct cprof_tbl_s *next;		/* next in chain */
  char cpath[CPROF_CPATH_MAX_LEN];	/* string with call path */
  int calls;				/* nr of executions of path */
  u64_t cycles;				/* execution time of path, in cycles */
};

int sprofile(int action, int size, int freq, int type, void *ctl_ptr,
	void *mem_ptr);

int cprofile(int action, int size, void *ctl_ptr, void *mem_ptr);

#endif /* PROFILE_H */

