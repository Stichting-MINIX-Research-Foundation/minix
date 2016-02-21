/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct rproc;
struct rprocupd;

/* exec.c */
int srv_execve(int proc_e, char *exec, size_t exec_len, char *progname,
	char *argv[], char **env);

/* main.c */
int main(void);

/* request.c */
int do_up(message *m);
int do_down(message *m);
int do_refresh(message *m);
int do_restart(message *m);
int do_clone(message *m);
int do_unclone(message *m);
int do_edit(message *m);
int do_shutdown(message *m);
void do_period(message *m);
int do_init_ready(message *m);
int do_update(message *m);
int do_upd_ready(message *m);
void do_sigchld(void);
int do_getsysinfo(message *m);
int do_lookup(message *m);
int do_sysctl(message *m);
int do_fi(message *m);

/* manager.c */
int check_call_permission(endpoint_t caller, int call, struct rproc
	*rp);
int copy_rs_start(endpoint_t src_e, char *src_rs_start, struct rs_start
	*rs_start);
int copy_label(endpoint_t src_e, char *src_label, size_t src_len, char
	*dst_label, size_t dst_len);
int init_state_data(endpoint_t src_e, int prepare_state,
	struct rs_state_data *src_rs_state_data,
	struct rs_state_data *dst_rs_state_data);
void build_cmd_dep(struct rproc *rp);
#define kill_service(rp, errstr, err) \
	kill_service_debug(__FILE__, __LINE__, rp, errstr, err)
int kill_service_debug(char *file, int line, struct rproc *rp, char
	*errstr, int err);
#define crash_service(rp) \
	crash_service_debug(__FILE__, __LINE__, rp)
int crash_service_debug(char *file, int line, struct rproc *rp);
#define cleanup_service(rp) \
	cleanup_service_debug(__FILE__, __LINE__, rp)
#define cleanup_service_now(rp) \
	do { struct rproc *rpt = rp; cleanup_service(rpt); cleanup_service(rpt); } while(0)
void cleanup_service_debug(char *file, int line,
	struct rproc *rp);
#define detach_service(rp) \
	detach_service_debug(__FILE__, __LINE__, rp)
void detach_service_debug(char *file, int line,
	struct rproc *rp);
int create_service(struct rproc *rp);
int clone_service(struct rproc *rp, int instance_flag, int init_flags);
int publish_service(struct rproc *rp);
int unpublish_service(struct rproc *rp);
int run_service(struct rproc *rp, int init_type, int init_flags);
int start_service(struct rproc *rp, int init_flags);
void stop_service(struct rproc *rp,int how);
void activate_service(struct rproc *rp, struct rproc *ex_rp);
void terminate_service(struct rproc *rp);
void restart_service(struct rproc *rp);
void inherit_service_defaults(struct rproc *def_rp,
	struct rproc *rp);
void get_service_instances(struct rproc *rp, struct rproc ***rps,
	int *length);
int read_exec(struct rproc *rp);
void share_exec(struct rproc *rp_src, struct rproc *rp_dst);
void free_exec(struct rproc *rp);
int init_slot(struct rproc *rp, struct rs_start *rs_start,
	endpoint_t source);
int edit_slot(struct rproc *rp, struct rs_start *rs_start,
	endpoint_t source);
int clone_slot(struct rproc *rp, struct rproc **clone_rpp);
void swap_slot(struct rproc **src_rpp, struct rproc **dst_rpp);
struct rproc* lookup_slot_by_label(char *label);
struct rproc* lookup_slot_by_pid(pid_t pid);
struct rproc* lookup_slot_by_dev_nr(dev_t dev_nr);
struct rproc* lookup_slot_by_domain(int domain);
struct rproc* lookup_slot_by_flags(int flags);
int alloc_slot(struct rproc **rpp);
void free_slot(struct rproc *rp);
char *get_next_label(char *ptr, char *label, char *caller_label);
void add_forward_ipc(struct rproc *rp, struct priv *privp);
void add_backward_ipc(struct rproc *rp, struct priv *privp);
void init_privs(struct rproc *rp, struct priv *privp);
void end_srv_init(struct rproc *rp);

/* update.c */
void rupdate_clear_upds(void);
void rupdate_add_upd(struct rprocupd* rpupd);
void rupdate_set_new_upd_flags(struct rprocupd* rpupd);
void rupdate_upd_init(struct rprocupd* rpupd, struct rproc *rp);
void rupdate_upd_clear(struct rprocupd* rpupd);
void rupdate_upd_move(struct rproc* src_rp, struct rproc* dst_rp);
#define request_prepare_update_service(rp, state) \
	request_prepare_update_service_debug(__FILE__, __LINE__, rp, state)
void request_prepare_update_service_debug(char *file, int line,
	struct rproc *rp, int state);
int srv_update(endpoint_t src_e, endpoint_t dst_e, int sys_upd_flags);
int update_service(struct rproc **src_rpp,
	struct rproc **dst_rpp, int swap_flag, int sys_upd_flags);
void rollback_service(struct rproc **src_rpp,
	struct rproc **dst_rpp);
void update_period(message *m_ptr);
int start_update_prepare(int allow_retries);
struct rprocupd* start_update_prepare_next(void);
int start_update(void);
int start_srv_update(struct rprocupd *rpupd);
int complete_srv_update(struct rprocupd *rpupd);
void end_srv_update(struct rprocupd *rpupd, int result, int reply_flag);
int abort_update_proc(int reason);
#define end_update(result, reply_flag) \
	end_update_debug(__FILE__, __LINE__, result, reply_flag)
void end_update_debug(char *file, int line,
        int result, int reply_flag);

/* utility.c */
int init_service(struct rproc *rp, int type, int flags);
int fi_service(struct rproc *rp);
void fill_send_mask(sys_map_t *send_mask, int set_bits);
void fill_call_mask( int *calls, int tot_nr_calls,
	bitchunk_t *call_mask, int call_base, int is_init);
#define srv_to_string(RP) srv_to_string_gen(RP, DEBUG)
char* srv_to_string_gen(struct rproc *rp, int is_verbose);
char* srv_upd_to_string(struct rprocupd *rpupd);
int rs_asynsend(struct rproc *rp, message *m_ptr, int no_reply);
int rs_receive_ticks(endpoint_t src, message *m_ptr,
    int *status_ptr, clock_t ticks);
void reply(endpoint_t who, struct rproc *rp, message *m_ptr);
void late_reply(struct rproc *rp, int code);
int rs_isokendpt(endpoint_t endpoint, int *proc);
int sched_init_proc(struct rproc *rp);
int update_sig_mgrs(struct rproc *rp, endpoint_t sig_mgr,
	endpoint_t bak_sig_mgr);
int rs_is_idle(void);
void rs_idle_period(void);
void print_services_status(void);
void print_update_status(void);

/* error.c */
char * init_strerror(int errnum);
char * lu_strerror(int errnum);

