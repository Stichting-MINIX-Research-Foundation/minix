/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct rproc;

/* exec.c */
_PROTOTYPE( int srv_execve, (int proc_e, 
	char *exec, size_t exec_len,  char *argv[], char **env));

/* main.c */
_PROTOTYPE( int main, (void));

/* request.c */
_PROTOTYPE( int do_up, (message *m));
_PROTOTYPE( int do_down, (message *m));
_PROTOTYPE( int do_refresh, (message *m));
_PROTOTYPE( int do_restart, (message *m));
_PROTOTYPE( int do_clone, (message *m));
_PROTOTYPE( int do_edit, (message *m));
_PROTOTYPE( int do_shutdown, (message *m));
_PROTOTYPE( void do_period, (message *m));
_PROTOTYPE( int do_init_ready, (message *m));
_PROTOTYPE( int do_update, (message *m));
_PROTOTYPE( int do_upd_ready, (message *m));
_PROTOTYPE( void do_sigchld, (void));
_PROTOTYPE( int do_getsysinfo, (message *m));
_PROTOTYPE( int do_lookup, (message *m));

/* manager.c */
_PROTOTYPE( int caller_is_root, (endpoint_t endpoint) );
_PROTOTYPE( int caller_can_control, (endpoint_t endpoint, char *label) );
_PROTOTYPE( int check_call_permission, (endpoint_t caller, int call,
	struct rproc *rp) );
_PROTOTYPE( int copy_rs_start, (endpoint_t src_e, char *src_rs_start,
	struct rs_start *rs_start) );
_PROTOTYPE( int copy_label, (endpoint_t src_e, char *src_label, size_t src_len,
	char *dst_label, size_t dst_len) );
_PROTOTYPE( void build_cmd_dep, (struct rproc *rp) );
_PROTOTYPE( int srv_fork, (void) );
_PROTOTYPE( int srv_kill, (pid_t pid, int sig) );
_PROTOTYPE( int srv_update, (endpoint_t src_e, endpoint_t dst_e) );
#define kill_service(rp, errstr, err) \
	kill_service_debug(__FILE__, __LINE__, rp, errstr, err)
_PROTOTYPE( int kill_service_debug, (char *file, int line, struct rproc *rp,
	char *errstr, int err) );
#define crash_service(rp) \
	crash_service_debug(__FILE__, __LINE__, rp)
_PROTOTYPE( int crash_service_debug, (char *file, int line, struct rproc *rp) );
#define cleanup_service(rp) \
	cleanup_service_debug(__FILE__, __LINE__, rp)
_PROTOTYPE( void cleanup_service_debug, (char *file, int line,
	struct rproc *rp) );
_PROTOTYPE( int create_service, (struct rproc *rp) );
_PROTOTYPE( int clone_service, (struct rproc *rp, int instance_flag) );
_PROTOTYPE( int publish_service, (struct rproc *rp) );
_PROTOTYPE( int unpublish_service, (struct rproc *rp) );
_PROTOTYPE( int run_service, (struct rproc *rp, int init_type) );
_PROTOTYPE( int start_service, (struct rproc *rp) );
_PROTOTYPE( void stop_service, (struct rproc *rp,int how) );
_PROTOTYPE( int update_service, (struct rproc **src_rpp,
	struct rproc **dst_rpp, int swap_flag) );
_PROTOTYPE( void activate_service, (struct rproc *rp, struct rproc *ex_rp) );
_PROTOTYPE( void terminate_service, (struct rproc *rp));
_PROTOTYPE( void restart_service, (struct rproc *rp) );
_PROTOTYPE( void inherit_service_defaults, (struct rproc *def_rp,
	struct rproc *rp) );
_PROTOTYPE( void get_service_instances, (struct rproc *rp, struct rproc ***rps,
	int *length) );
_PROTOTYPE( int read_exec, (struct rproc *rp) );
_PROTOTYPE( void share_exec, (struct rproc *rp_src, struct rproc *rp_dst) );
_PROTOTYPE( void free_exec, (struct rproc *rp) );
_PROTOTYPE( int init_slot, (struct rproc *rp, struct rs_start *rs_start,
	endpoint_t source) );
_PROTOTYPE( int edit_slot, (struct rproc *rp, struct rs_start *rs_start,
	endpoint_t source) );
_PROTOTYPE( int clone_slot, (struct rproc *rp, struct rproc **clone_rpp) );
_PROTOTYPE( void swap_slot, (struct rproc **src_rpp, struct rproc **dst_rpp) );
_PROTOTYPE( struct rproc* lookup_slot_by_label, (char *label) );
_PROTOTYPE( struct rproc* lookup_slot_by_pid, (pid_t pid) );
_PROTOTYPE( struct rproc* lookup_slot_by_dev_nr, (dev_t dev_nr) );
_PROTOTYPE( struct rproc* lookup_slot_by_flags, (int flags) );
_PROTOTYPE( int alloc_slot, (struct rproc **rpp) );
_PROTOTYPE( void free_slot, (struct rproc *rp) );
_PROTOTYPE( char *get_next_label, (char *ptr, char *label, char *caller_label));
_PROTOTYPE( void add_forward_ipc, (struct rproc *rp, struct priv *privp) );
_PROTOTYPE( void add_backward_ipc, (struct rproc *rp, struct priv *privp) );
_PROTOTYPE( void init_privs, (struct rproc *rp, struct priv *privp) );
_PROTOTYPE( void update_period, (message *m_ptr) );
_PROTOTYPE( void end_update, (int result, int reply_flag) );

/* utility.c */
_PROTOTYPE( int init_service, (struct rproc *rp, int type));
_PROTOTYPE(void fill_call_mask, ( int *calls, int tot_nr_calls,
	bitchunk_t *call_mask, int call_base, int is_init));
_PROTOTYPE( char* srv_to_string, (struct rproc *rp));
_PROTOTYPE( void reply, (endpoint_t who, struct rproc *rp, message *m_ptr));
_PROTOTYPE( void late_reply, (struct rproc *rp, int code));
_PROTOTYPE( int rs_isokendpt, (endpoint_t endpoint, int *proc));
_PROTOTYPE( int sched_init_proc, (struct rproc *rp));
_PROTOTYPE( int update_sig_mgrs, (struct rproc *rp, endpoint_t sig_mgr,
	endpoint_t bak_sig_mgr));

/* error.c */
_PROTOTYPE( char * init_strerror, (int errnum) );
_PROTOTYPE( char * lu_strerror, (int errnum) );

