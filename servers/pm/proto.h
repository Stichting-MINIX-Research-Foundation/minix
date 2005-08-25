/* Function prototypes. */

struct mproc;
struct stat;
struct mem_map;
struct memory;

#include <timers.h>

/* alloc.c */
_PROTOTYPE( phys_clicks alloc_mem, (phys_clicks clicks)			);
_PROTOTYPE( void free_mem, (phys_clicks base, phys_clicks clicks)	);
_PROTOTYPE( void mem_init, (struct memory *chunks, phys_clicks *free)	);
#if ENABLE_SWAP
_PROTOTYPE( int swap_on, (char *file, u32_t offset, u32_t size)	);
_PROTOTYPE( int swap_off, (void)					);
_PROTOTYPE( void swap_in, (void)					);
_PROTOTYPE( void swap_inqueue, (struct mproc *rmp)			);
#else /* !SWAP */
#define swap_in()			((void)0)
#define swap_inqueue(rmp)		((void)0)
#endif /* !SWAP */

/* break.c */
_PROTOTYPE( int adjust, (struct mproc *rmp,
			vir_clicks data_clicks, vir_bytes sp)		);
_PROTOTYPE( int do_brk, (void)						);
_PROTOTYPE( int size_ok, (int file_type, vir_clicks tc, vir_clicks dc,
			vir_clicks sc, vir_clicks dvir, vir_clicks s_vir) );

/* devio.c */
_PROTOTYPE( int do_dev_io, (void) );
_PROTOTYPE( int do_dev_io, (void) );

/* dmp.c */
_PROTOTYPE( int do_fkey_pressed, (void)						);

/* exec.c */
_PROTOTYPE( int do_exec, (void)						);
_PROTOTYPE( void rw_seg, (int rw, int fd, int proc, int seg,
						phys_bytes seg_bytes)	);
_PROTOTYPE( struct mproc *find_share, (struct mproc *mp_ign, Ino_t ino,
			Dev_t dev, time_t ctime)			);

/* forkexit.c */
_PROTOTYPE( int do_fork, (void)						);
_PROTOTYPE( int do_pm_exit, (void)					);
_PROTOTYPE( int do_waitpid, (void)					);
_PROTOTYPE( void pm_exit, (struct mproc *rmp, int exit_status)		);

/* getset.c */
_PROTOTYPE( int do_getset, (void)					);

/* main.c */
_PROTOTYPE( int main, (void)						);

/* misc.c */
_PROTOTYPE( int do_reboot, (void)					);
_PROTOTYPE( int do_getsysinfo, (void)					);
_PROTOTYPE( int do_getprocnr, (void)					);
_PROTOTYPE( int do_svrctl, (void)					);
_PROTOTYPE( int do_allocmem, (void)					);
_PROTOTYPE( int do_freemem, (void)					);
_PROTOTYPE( int do_getsetpriority, (void)					);

#if (MACHINE == MACINTOSH)
_PROTOTYPE( phys_clicks start_click, (void)				);
#endif

_PROTOTYPE( void setreply, (int proc_nr, int result)			);

/* signal.c */
_PROTOTYPE( int do_alarm, (void)					);
_PROTOTYPE( int do_kill, (void)						);
_PROTOTYPE( int ksig_pending, (void)					);
_PROTOTYPE( int do_pause, (void)					);
_PROTOTYPE( int set_alarm, (int proc_nr, int sec)			);
_PROTOTYPE( int check_sig, (pid_t proc_id, int signo)			);
_PROTOTYPE( void sig_proc, (struct mproc *rmp, int sig_nr)		);
_PROTOTYPE( int do_sigaction, (void)					);
_PROTOTYPE( int do_sigpending, (void)					);
_PROTOTYPE( int do_sigprocmask, (void)					);
_PROTOTYPE( int do_sigreturn, (void)					);
_PROTOTYPE( int do_sigsuspend, (void)					);
_PROTOTYPE( void check_pending, (struct mproc *rmp)			);

/* time.c */
_PROTOTYPE( int do_stime, (void)					);
_PROTOTYPE( int do_time, (void)						);
_PROTOTYPE( int do_times, (void)					);
_PROTOTYPE( int do_gettimeofday, (void)					);

/* timers.c */
_PROTOTYPE( void pm_set_timer, (timer_t *tp, int delta, 
	tmr_func_t watchdog, int arg));
_PROTOTYPE( void pm_expire_timers, (clock_t now));
_PROTOTYPE( void pm_cancel_timer, (timer_t *tp));

/* trace.c */
_PROTOTYPE( int do_trace, (void)					);
_PROTOTYPE( void stop_proc, (struct mproc *rmp, int sig_nr)		);

/* utility.c */
_PROTOTYPE( pid_t get_free_pid, (void)					);
_PROTOTYPE( int allowed, (char *name_buf, struct stat *s_buf, int mask)	);
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( void panic, (char *who, char *mess, int num)		);
_PROTOTYPE( void tell_fs, (int what, int p1, int p2, int p3)		);
_PROTOTYPE( int get_stack_ptr, (int proc_nr, vir_bytes *sp)		);
_PROTOTYPE( int get_mem_map, (int proc_nr, struct mem_map *mem_map)	);
_PROTOTYPE( char *find_param, (const char *key));
_PROTOTYPE( int proc_from_pid, (pid_t p));

