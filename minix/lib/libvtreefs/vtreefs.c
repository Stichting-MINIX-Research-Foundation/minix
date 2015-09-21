/* VTreeFS - vtreefs.c - initialization and message loop */

#include "inc.h"

static unsigned int inodes;
static struct inode_stat *root_stat;
static index_t root_entries;
static size_t buf_size;
static size_t extra_size;

/*
 * Initialize internal state.  This is the only place where dynamic memory
 * allocation takes place.
 */
static int
init_server(int __unused type, sef_init_info_t * __unused info)
{
	int r;

	/* Initialize the virtual tree. */
	if ((r = init_inodes(inodes, root_stat, root_entries)) != OK)
		panic("init_inodes failed: %d", r);

	/* Initialize extra data. */
	if ((r = init_extra(inodes, extra_size)) != OK)
		panic("init_extra failed: %d", r);

	/* Initialize the I/O buffer. */
	if ((r = init_buf(buf_size)) != OK)
		panic("init_buf failed: %d", r);

	return OK;
}

/*
 * We received a signal.
 */
static void
got_signal(int sig)
{

	if (sig != SIGTERM)
		return;

	fsdriver_terminate();
}

/*
 * SEF initialization.
 */
static void
sef_local_startup(void)
{
	sef_setcb_init_fresh(init_server);
	sef_setcb_init_restart(SEF_CB_INIT_RESTART_STATEFUL);

	sef_setcb_signal_handler(got_signal);

	sef_startup();
}

/*
 * We have received a message that is not a file system request from VFS.
 * Call the message hook, if there is one.
 */
void
fs_other(const message * m_ptr, int ipc_status)
{
	message msg;

	if (vtreefs_hooks->message_hook != NULL) {
		/*
		 * Not all of vtreefs's users play nice with the message, so
		 * make a copy to allow it to be modified.
		 */
		msg = *m_ptr;

		vtreefs_hooks->message_hook(&msg, ipc_status);
	}
}

/*
 * This is the main routine of this service.  It uses the main loop as provided
 * by the fsdriver library.  The routine returns once the file system has been
 * unmounted and the process is signaled to exit.
 */
void
run_vtreefs(struct fs_hooks * hooks, unsigned int nr_inodes,
	size_t inode_extra, struct inode_stat * istat,
	index_t nr_indexed_entries, size_t bufsize)
{

	/*
	 * Use global variables to work around the inability to pass parameters
	 * through SEF to the initialization function..
	 */
	vtreefs_hooks = hooks;
	inodes = nr_inodes;
	extra_size = inode_extra;
	root_stat = istat;
	root_entries = nr_indexed_entries;
	buf_size = bufsize;

	sef_local_startup();

	fsdriver_task(&vtreefs_table);

	cleanup_buf();
	cleanup_inodes();
}
