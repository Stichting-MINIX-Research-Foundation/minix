/* Function prototypes. */

struct mproc;
struct stat;
struct mem_map;
struct memory;

#include <timers.h>

/* alarm.c */
_PROTOTYPE( int do_alarm, (void)					);
_PROTOTYPE( int do_itimer, (void)					);
_PROTOTYPE( void set_alarm, (struct mproc *rmp, clock_t ticks)		);
_PROTOTYPE( void check_vtimer, (int proc_nr, int sig)			);

/* break.c */
_PROTOTYPE( int do_brk, (void)						);

/* dma.c */
_PROTOTYPE( int do_adddma, (void)					);
_PROTOTYPE( int do_deldma, (void)					);
_PROTOTYPE( int do_getdma, (void)					);

/* exec.c */
_PROTOTYPE( int do_exec, (void)						);
_PROTOTYPE( int do_exec_newmem, (void)					);
_PROTOTYPE( int do_execrestart, (void)					);
_PROTOTYPE( void exec_restart, (struct mproc *rmp, int result,
				vir_bytes pc)                          );

/* forkexit.c */
_PROTOTYPE( int do_fork, (void)						);
_PROTOTYPE( int do_srv_fork, (void)					);
_PROTOTYPE( int do_exit, (void)						);
_PROTOTYPE( void exit_proc, (struct mproc *rmp, int exit_status,
	int dump_core)							);
_PROTOTYPE( void exit_restart, (struct mproc *rmp, int dump_core)	);
_PROTOTYPE( int do_waitpid, (void)					);
_PROTOTYPE( int wait_test, (struct mproc *rmp, struct mproc *child)	);

/* getset.c */
_PROTOTYPE( int do_get, (void)						);
_PROTOTYPE( int do_set, (void)						);

/* main.c */
_PROTOTYPE( int main, (void)						);
_PROTOTYPE( void setreply, (int proc_nr, int result)			);

/* mcontext.c */
_PROTOTYPE( int do_getmcontext, (void)					);
_PROTOTYPE( int do_setmcontext, (void)					);

/* misc.c */
_PROTOTYPE( int do_reboot, (void)					);
_PROTOTYPE( int do_procstat, (void)					);
_PROTOTYPE( int do_sysuname, (void)					);
_PROTOTYPE( int do_getsysinfo, (void)					);
_PROTOTYPE( int do_getsysinfo_up, (void)					);
_PROTOTYPE( int do_getprocnr, (void)					);
_PROTOTYPE( int do_getepinfo, (void)					);
_PROTOTYPE( int do_svrctl, (void)					);
_PROTOTYPE( int do_getsetpriority, (void)				);

/* schedule.c */
_PROTOTYPE( void sched_init, (void)					);
_PROTOTYPE( int sched_start_user, (endpoint_t ep, struct mproc *rmp)	);
_PROTOTYPE( int sched_nice, (struct mproc *rmp, int nice)		);

/* profile.c */
_PROTOTYPE( int do_sprofile, (void)					);
_PROTOTYPE( int do_cprofile, (void)					);

/* signal.c */
_PROTOTYPE( int do_kill, (void)						);
_PROTOTYPE( int do_srv_kill, (void)					);
_PROTOTYPE( int process_ksig, (endpoint_t proc_nr_e, int signo)		);
_PROTOTYPE( int do_pause, (void)					);
_PROTOTYPE( int check_sig, (pid_t proc_id, int signo, int ksig)		);
_PROTOTYPE( void sig_proc, (struct mproc *rmp, int signo, int trace,
							int ksig)	);
_PROTOTYPE( int do_sigaction, (void)					);
_PROTOTYPE( int do_sigpending, (void)					);
_PROTOTYPE( int do_sigprocmask, (void)					);
_PROTOTYPE( int do_sigreturn, (void)					);
_PROTOTYPE( int do_sigsuspend, (void)					);
_PROTOTYPE( void check_pending, (struct mproc *rmp)			);
_PROTOTYPE( void restart_sigs, (struct mproc *rmp)			);
_PROTOTYPE( void vm_notify_sig_wrapper, (endpoint_t ep)			);

/* time.c */
_PROTOTYPE( int do_stime, (void)					);
_PROTOTYPE( int do_time, (void)						);
_PROTOTYPE( int do_times, (void)					);

/* trace.c */
_PROTOTYPE( int do_trace, (void)					);
_PROTOTYPE( void stop_proc, (struct mproc *rmp, int sig_nr)		);

/* utility.c */
_PROTOTYPE( pid_t get_free_pid, (void)					);
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( char *find_param, (const char *key)				);
_PROTOTYPE( struct mproc *find_proc, (pid_t lpid)			);
_PROTOTYPE( int nice_to_priority, (int nice, unsigned *new_q)		);
_PROTOTYPE( int pm_isokendpt, (int ep, int *proc)			);
_PROTOTYPE( void tell_vfs, (struct mproc *rmp, message *m_ptr)		);
