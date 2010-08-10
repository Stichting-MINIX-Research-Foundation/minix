/* VTreeFS - vtreefs.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

FORWARD _PROTOTYPE( int get_work, (void)				);
FORWARD _PROTOTYPE( void send_reply, (int err)				);
FORWARD _PROTOTYPE( void got_signal, (int signal)			);

PRIVATE unsigned int inodes;
PRIVATE struct inode_stat *root_stat;
PRIVATE index_t root_entries;

/*===========================================================================*
 *				init_server				     *
 *===========================================================================*/
PRIVATE int init_server(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	/* Initialize internal state, and register with VFS.
	 */
	int r;

	/* Initialize the virtual tree. */
	init_inodes(inodes, root_stat, root_entries);

	/* Tell VFS that we are here. */
	fs_m_out.m_type = FS_READY;

	if ((r = send(VFS_PROC_NR, &fs_m_out)) != OK)
		panic(__FILE__, "error sending login to VFS", r);

	/* Do not yet allow any requests except REQ_READSUPER. */
	fs_mounted = FALSE;

	return OK;
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup(void)
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
PUBLIC void start_vtreefs(struct fs_hooks *hooks, unsigned int nr_inodes,
		struct inode_stat *stat, index_t nr_indexed_entries)
{
	/* This is the main routine of this service. The main loop consists of
	 * three major activities: getting new work, processing the work, and
	 * sending the reply. The loop exits when the process is signaled to
	 * exit; due to limitations of SEF, it can not return to the caller.
	 */
	int call_nr, err;

	/* Use global variables to work around the inability to pass parameters
	 * through SEF to the initialization function..
	 */
	vtreefs_hooks = hooks;
	inodes = nr_inodes;
	root_stat = stat;
	root_entries = nr_indexed_entries;

	sef_local_startup();

	for (;;) {
		call_nr = get_work();

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
			call_nr -= VFS_BASE;

			if (call_nr >= 0 && call_nr < NREQS) {
				err = (*fs_call_vec[call_nr])();
			} else {
				err = ENOSYS;
			}
		}
		else err = EINVAL;

		send_reply(err);
	}
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE int get_work(void)
{
	/* Retrieve work. Return the call number.
	 */
	int r;

	if ((r = sef_receive(ANY, &fs_m_in)) != OK)
		panic(__FILE__, "receive failed", r);

	return fs_m_in.m_type;
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
PRIVATE void send_reply(int err)
{
	/* Send a reply to the caller.
	 */
	int r;

	fs_m_out.m_type = err;

	if ((r = send(fs_m_in.m_source, &fs_m_out)) != OK)
		panic(__FILE__, "unable to send reply", r);
}

/*===========================================================================*
 *				got_signal				     *
 *===========================================================================*/
PRIVATE void got_signal(int signal)
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
