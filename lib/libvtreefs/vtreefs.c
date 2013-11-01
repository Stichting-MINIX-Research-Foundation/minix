/* VTreeFS - vtreefs.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

static int get_work(void);
static void send_reply(int err, int transid);
static void got_signal(int signal);

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

	/* Do not yet allow any requests except REQ_READSUPER. */
	fs_mounted = FALSE;

	return OK;
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
 *				start_vtreefs				     *
 *===========================================================================*/
void start_vtreefs(struct fs_hooks *hooks, unsigned int nr_inodes,
		struct inode_stat *stat, index_t nr_indexed_entries)
{
	/* This is the main routine of this service. The main loop consists of
	 * three major activities: getting new work, processing the work, and
	 * sending the reply. The loop exits when the process is signaled to
	 * exit; due to limitations of SEF, it can not return to the caller.
	 */
	int call_nr, err, transid;

	/* Use global variables to work around the inability to pass parameters
	 * through SEF to the initialization function..
	 */
	vtreefs_hooks = hooks;
	inodes = nr_inodes;
	root_stat = stat;
	root_entries = nr_indexed_entries;

	sef_local_startup();

	for (;;) {
		get_work();

		transid = TRNS_GET_ID(fs_m_in.m_type);
		fs_m_in.m_type = TRNS_DEL_ID(fs_m_in.m_type);
		if (fs_m_in.m_type == 0) {
			assert(!IS_VFS_FS_TRANSID(transid));
			fs_m_in.m_type = transid;	/* Backwards compat. */
			transid = 0;
		} else
			assert(IS_VFS_FS_TRANSID(transid));

		call_nr = fs_m_in.m_type;

		if (fs_m_in.m_source != VFS_PROC_NR) {
			if (vtreefs_hooks->message_hook != NULL) {
				/* If the request is not among the recognized
				 * requests, call the message hook.
				 */
				vtreefs_hooks->message_hook(&fs_m_in);
			}

			continue;
		}

		if (fs_mounted || call_nr == REQ_READSUPER) {
			call_nr -= FS_BASE;

			if (call_nr >= 0 && call_nr < NREQS) {
				err = (*fs_call_vec[call_nr])();
			} else {
				err = ENOSYS;
			}
		}
		else err = EINVAL;

		send_reply(err, transid);
	}
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
static int get_work(void)
{
	/* Retrieve work. Return the call number.
	 */
	int r;

	if ((r = sef_receive(ANY, &fs_m_in)) != OK)
		panic("receive failed: %d", r);

	return fs_m_in.m_type;
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
static void send_reply(int err, int transid)
{
	/* Send a reply to the caller.
	 */
	int r;

	fs_m_out.m_type = err;
	if (IS_VFS_FS_TRANSID(transid)) {
		fs_m_out.m_type = TRNS_ADD_ID(fs_m_out.m_type, transid);
	}

	if ((r = ipc_send(fs_m_in.m_source, &fs_m_out)) != OK)
		panic("unable to send reply: %d", r);
}

/*===========================================================================*
 *				got_signal				     *
 *===========================================================================*/
static void got_signal(int signal)
{
	/* We received a signal. If it is a termination signal, and the file
	 * system has already been unmounted, clean up and exit.
	 */

	if (signal != SIGTERM)
		return;

	if (fs_mounted)
		return;

	cleanup_inodes();

	exit(0);
}
