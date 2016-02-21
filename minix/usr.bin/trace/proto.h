
/* call.c */
void put_endpoint(struct trace_proc *proc, const char *name, endpoint_t endpt);
void put_equals(struct trace_proc *proc);
void put_result(struct trace_proc *proc);
int default_out(struct trace_proc *proc, const message *m_out);
void default_in(struct trace_proc *proc, const message *m_out,
	const message *m_in, int failed);
int call_enter(struct trace_proc *proc, int show_stack);
void call_leave(struct trace_proc *proc, int skip);
void call_replay(struct trace_proc *proc);
const char *call_name(struct trace_proc *proc);
int call_errno(struct trace_proc *proc, int *err);

/* error.c */
const char *get_error_name(int err);

/* escape.c */
const char *get_escape(char c);

/* format.c */
void format_reset(struct trace_proc *proc);
void format_set_sep(struct trace_proc *proc, const char *sep);
void format_push_sep(struct trace_proc *proc);
void put_field(struct trace_proc *proc, const char *name, const char *text);
void put_open(struct trace_proc *proc, const char *name, int flags,
	const char *string, const char *separator);
void put_close(struct trace_proc *proc, const char *string);
void put_fmt(struct trace_proc *proc, const char *fmt, ...)
	__attribute__((__format__(__printf__, 2, 3)));
void put_value(struct trace_proc *proc, const char *name, const char *fmt, ...)
	__attribute__((__format__(__printf__, 3, 4)));
int put_open_struct(struct trace_proc *proc, const char *name, int flags,
	vir_bytes addr, void *ptr, size_t size);
void put_close_struct(struct trace_proc *proc, int all);
void put_ptr(struct trace_proc *proc, const char *name, vir_bytes addr);
void put_buf(struct trace_proc *proc, const char *name, int flags,
	vir_bytes addr, ssize_t size);
void put_flags(struct trace_proc *proc, const char *name,
	const struct flags *fp, unsigned int num, const char *fmt,
	unsigned int value);
void put_tail(struct trace_proc * proc, unsigned int count,
	unsigned int printed);

/* ioctl.c */
void put_ioctl_req(struct trace_proc *proc, const char *name,
	unsigned long req, int is_svrctl);
int put_ioctl_arg_out(struct trace_proc *proc, const char *name,
	unsigned long req, vir_bytes addr, int is_svrctl);
void put_ioctl_arg_in(struct trace_proc *proc, const char *name, int failed,
	unsigned long req, vir_bytes addr, int is_svrctl);

/* kernel.c */
int kernel_check(pid_t pid);
int kernel_get_name(pid_t pid, char *name, size_t size);
int kernel_is_service(pid_t pid);
int kernel_get_syscall(pid_t pid, reg_t reg[3]);
int kernel_get_retreg(pid_t pid, reg_t *retreg);
vir_bytes kernel_get_stacktop(void);
int kernel_get_context(pid_t pid, reg_t *pc, reg_t *sp, reg_t *fp);
void kernel_put_stacktrace(struct trace_proc * proc);

/* mem.c */
int mem_get_data(pid_t pid, vir_bytes addr, void *ptr, size_t len);
int mem_get_user(pid_t pid, vir_bytes addr, void *ptr, size_t len);

/* pm.c */
void put_struct_timeval(struct trace_proc *proc, const char *name, int flags,
	vir_bytes addr);
void put_time(struct trace_proc *proc, const char *name, time_t time);
void put_groups(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, int count);

/* output.c */
int output_init(const char *file);
int output_error(void);
void output_flush(void);
void record_start(struct trace_proc *proc);
void record_stop(struct trace_proc *proc);
void record_clear(struct trace_proc *proc);
int record_replay(struct trace_proc *proc);
void put_newline(void);
void put_text(struct trace_proc *proc, const char *text);
void put_space(struct trace_proc *proc);
void put_align(struct trace_proc *proc);

/* proc.c */
void proc_init(void);
struct trace_proc *proc_add(pid_t pid);
struct trace_proc *proc_get(pid_t pid);
void proc_del(struct trace_proc *proc);
struct trace_proc *proc_next(struct trace_proc *last);
unsigned int proc_count(void);

/* signal.c */
const char *get_signal_name(int sig);

/* trace.c */
extern int timestamps;
extern int allnames;
extern unsigned int verbose;
extern unsigned int valuesonly;

/* service */
const struct calls pm_calls;
const struct calls vfs_calls;
const struct calls rs_calls;
const struct calls mib_calls;
const struct calls vm_calls;
const struct calls ipc_calls;

/* service/vfs.c */
void put_fd(struct trace_proc *proc, const char *name, int fd);
void put_dev(struct trace_proc *proc, const char *name, dev_t dev);
void put_in_addr(struct trace_proc *proc, const char *name, struct in_addr in);
void put_socket_type(struct trace_proc *proc, const char *name, int type);
void put_socket_family(struct trace_proc *proc, const char *name, int family);
void put_cmsg_type(struct trace_proc *proc, const char *name, int type);
void put_shutdown_how(struct trace_proc *proc, const char *name, int how);

/* ioctl/block.c */
const char *block_ioctl_name(unsigned long req);
int block_ioctl_arg(struct trace_proc *proc, unsigned long req, void *ptr,
	int dir);

/* ioctl/char.c */
const char *char_ioctl_name(unsigned long req);
int char_ioctl_arg(struct trace_proc *proc, unsigned long req, void *ptr,
	int dir);

/* ioctl/net.c */
const char *net_ioctl_name(unsigned long req);
int net_ioctl_arg(struct trace_proc *proc, unsigned long req, void *ptr,
	int dir);

/* ioctl/svrctl.c */
const char *svrctl_name(unsigned long req);
int svrctl_arg(struct trace_proc *proc, unsigned long req, void *ptr, int dir);
