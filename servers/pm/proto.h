/* Function prototypes. */

struct mproc;
struct stat;
struct mem_map;
struct memory;

#include <timers.h>

/* alarm.c */
int do_alarm(void);
int do_itimer(void);
void set_alarm(struct mproc *rmp, clock_t ticks);
void check_vtimer(int proc_nr, int sig);

/* break.c */
int do_brk(void);

/* exec.c */
int do_exec(void);
int do_newexec(void);
int do_execrestart(void);
void exec_restart(struct mproc *rmp, int result, vir_bytes pc, vir_bytes sp);

/* forkexit.c */
int do_fork(void);
int do_srv_fork(void);
int do_exit(void);
void exit_proc(struct mproc *rmp, int exit_status, int dump_core);
void exit_restart(struct mproc *rmp, int dump_core);
int do_waitpid(void);
int wait_test(struct mproc *rmp, struct mproc *child);

/* getset.c */
int do_get(void);
int do_set(void);

/* main.c */
int main(void);
void setreply(int proc_nr, int result);

/* mcontext.c */
int do_getmcontext(void);
int do_setmcontext(void);
#if ! USE_MCONTEXT
#define do_getmcontext no_sys
#define do_setmcontext no_sys
#endif

/* misc.c */
int do_reboot(void);
int do_sysuname(void);
int do_getsysinfo(void);
int do_getprocnr(void);
int do_getepinfo(void);
int do_getepinfo_o(void);
int do_svrctl(void);
int do_getsetpriority(void);
int do_getrusage(void);

/* schedule.c */
void sched_init(void);
int sched_start_user(endpoint_t ep, struct mproc *rmp);
int sched_nice(struct mproc *rmp, int nice);

/* profile.c */
int do_sprofile(void);
int do_cprofile(void);

/* signal.c */
int do_kill(void);
int do_srv_kill(void);
int process_ksig(endpoint_t proc_nr_e, int signo);
int do_pause(void);
int check_sig(pid_t proc_id, int signo, int ksig);
void sig_proc(struct mproc *rmp, int signo, int trace, int ksig);
int do_sigaction(void);
int do_sigpending(void);
int do_sigprocmask(void);
int do_sigreturn(void);
int do_sigsuspend(void);
void check_pending(struct mproc *rmp);
void restart_sigs(struct mproc *rmp);
void vm_notify_sig_wrapper(endpoint_t ep);

/* time.c */
int do_stime(void);
int do_time(void);
int do_times(void);
int do_getres(void);
int do_gettime(void);
int do_settime(void);

/* trace.c */
int do_trace(void);
void stop_proc(struct mproc *rmp, int sig_nr);
#if ! USE_TRACE
#define do_trace no_sys
#define stop_proc no_sys
#endif

/* utility.c */
pid_t get_free_pid(void);
int no_sys(void);
char *find_param(const char *key);
struct mproc *find_proc(pid_t lpid);
int nice_to_priority(int nice, unsigned *new_q);
int pm_isokendpt(int ep, int *proc);
void tell_vfs(struct mproc *rmp, message *m_ptr);
