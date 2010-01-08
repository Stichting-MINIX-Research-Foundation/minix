/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct rproc;

/* exec.c */
_PROTOTYPE( int dev_execve, (int proc_e, 
	char *exec, size_t exec_len,  char *argv[], char **env));

/* main.c */
_PROTOTYPE( int main, (void));

/* manager.c */
_PROTOTYPE( int do_up, (message *m));
_PROTOTYPE( int do_down, (message *m));
_PROTOTYPE( int do_refresh, (message *m));
_PROTOTYPE( int do_restart, (message *m));
_PROTOTYPE( int do_lookup, (message *m));
_PROTOTYPE( int do_shutdown, (message *m));
_PROTOTYPE( void do_period, (message *m));
_PROTOTYPE( int do_init_ready, (message *m));
_PROTOTYPE( int do_update, (message *m));
_PROTOTYPE( int do_upd_ready, (message *m));
_PROTOTYPE( void do_exit, (message *m));
_PROTOTYPE( int do_getsysinfo, (message *m));

/* utility.c */
_PROTOTYPE( int init_service, (struct rproc *rp, int type));
_PROTOTYPE( int publish_service, (struct rproc *rp));
_PROTOTYPE(void fill_call_mask, ( int *calls, int tot_nr_calls,
    bitchunk_t *call_mask, int call_base, int is_init));

/* memory.c */
_PROTOTYPE( void* rs_startup_sbrk, (size_t size));
_PROTOTYPE( void* rs_startup_sbrk_synch, (size_t size));
_PROTOTYPE( int rs_startup_segcopy, (endpoint_t src_proc, int src_s,
    int dst_s, vir_bytes dst_vir, phys_bytes bytes));

