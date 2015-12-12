/* Function prototypes. */

struct mproc;

#include <minix/timers.h>

/* alarm.c */
int do_itimer(void);
void set_alarm(struct mproc *rmp, clock_t ticks);
void check_vtimer(int proc_nr, int sig);

/* event.c */
int do_proceventmask(void);
int do_proc_event_reply(void);
void publish_event(struct mproc *rmp);

/* exec.c */
int do_exec(void);
int do_newexec(void);
int do_execrestart(void);
void exec_restart(struct mproc *rmp, int result, vir_bytes pc, vir_bytes sp,
	vir_bytes ps_str);

/* forkexit.c */
int do_fork(void);
int do_srv_fork(void);
int do_exit(void);
void exit_proc(struct mproc *rmp, int exit_status, int dump_core);
void exit_restart(struct mproc *rmp);
int do_wait4(void);
int wait_test(struct mproc *rmp, struct mproc *child);

/* getset.c */
int do_get(void);
int do_set(void);

/* main.c */
int main(void);
void reply(int proc_nr, int result);

/* mcontext.c */
int do_getmcontext(void);
int do_setmcontext(void);

/* misc.c */
int do_reboot(void);
int do_sysuname(void);
int do_getsysinfo(void);
int do_getprocnr(void);
int do_getepinfo(void);
int do_svrctl(void);
int do_getsetpriority(void);
int do_getrusage(void);

/* schedule.c */
void sched_init(void);
int sched_start_user(endpoint_t ep, struct mproc *rmp);
int sched_nice(struct mproc *rmp, int nice);

/* profile.c */
int do_sprofile(void);

/* signal.c */
int do_kill(void);
int do_srv_kill(void);
int process_ksig(endpoint_t proc_nr_e, int signo);
int check_sig(pid_t proc_id, int signo, int ksig);
void sig_proc(struct mproc *rmp, int signo, int trace, int ksig);
int do_sigaction(void);
int do_sigpending(void);
int do_sigprocmask(void);
int do_sigreturn(void);
int do_sigsuspend(void);
void check_pending(struct mproc *rmp);
void restart_sigs(struct mproc *rmp);

/* time.c */
int do_stime(void);
int do_time(void);
int do_getres(void);
int do_gettime(void);
int do_settime(void);

/* trace.c */
int do_trace(void);
void trace_stop(struct mproc *rmp, int signo);

/* utility.c */
pid_t get_free_pid(void);
char *find_param(const char *key);
struct mproc *find_proc(pid_t lpid);
int nice_to_priority(int nice, unsigned *new_q);
int pm_isokendpt(int ep, int *proc);
void tell_vfs(struct mproc *rmp, message *m_ptr);
void set_rusage_times(struct rusage *r_usage, clock_t user_time,
	clock_t sys_time);
