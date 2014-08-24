/* VTreeFS - vtreefs.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

static unsigned int inodes;
static struct inode_stat *root_stat;
static index_t root_entries;

/*===========================================================================*
 *				init_server				     *
 *===========================================================================*/
static int init_server(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	/* Initialize internal state, and register with VFS.
	 */

	/* Initialize the virtual tree. */
	init_inodes(inodes, root_stat, root_entries);

	return OK;
}

/*===========================================================================*
 *				got_signal				     *
 *===========================================================================*/
static void got_signal(int signal)
{
	/* We received a signal.
	 */

	if (signal != SIGTERM)
		return;

	fsdriver_terminate();
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
	sef_setcb_init_fresh(init_server);
	sef_setcb_init_restart(init_server);

	sef_setcb_signal_handler(got_signal);

	/* No support for live update yet. */

	sef_startup();
}

/*===========================================================================*
 *				fs_other				     *
 *===========================================================================*/
void fs_other(const message *m_ptr, int __unused ipc_status)
{
	/* We received a message that is not among the recognized file system
	 * requests.  Call the message hook, if there is one.
	 */
	message msg;

	if (vtreefs_hooks->message_hook != NULL) {
		/* Not all of vtreefs's users play nice with the message, so
		 * make a copy to allow it to be modified.
		 */
		msg = *m_ptr;

		vtreefs_hooks->message_hook(&msg);
	}
}

/*===========================================================================*
 *				start_vtreefs				     *
 *===========================================================================*/
void start_vtreefs(struct fs_hooks *hooks, unsigned int nr_inodes,
		struct inode_stat *stat, index_t nr_indexed_entries)
{
	/* This is the main routine of this service. It uses the main loop as
	 * provided by the fsdriver library. The routine returns once the file
	 * system has been unmounted and the process is signaled to exit.
	 */

	/* Use global variables to work around the inability to pass parameters
	 * through SEF to the initialization function..
	 */
	vtreefs_hooks = hooks;
	inodes = nr_inodes;
	root_stat = stat;
	root_entries = nr_indexed_entries;

	sef_local_startup();

	fsdriver_task(&vtreefs_table);

	cleanup_inodes();
}
