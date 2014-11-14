
#include "fsdriver.h"

/* Library-local variables. */
dev_t fsdriver_device;
ino_t fsdriver_root;
int fsdriver_mounted = FALSE;

static int fsdriver_running;

/*
 * Process an incoming VFS request, and send a reply.  If the message is not
 * a file system request from VFS, pass it on to the generic message handler.
 * Multithreaded file systems should indicate that the reply is to be sent to
 * VFS asynchronously.
 */
void
fsdriver_process(const struct fsdriver * __restrict fdp,
	const message * __restrict m_ptr, int ipc_status, int asyn_reply)
{
	message m_out;
	unsigned int call_nr;
	int r, transid;

	/* Is this a file system request at all? */
	if (is_ipc_notify(ipc_status) || m_ptr->m_source != VFS_PROC_NR) {
		if (fdp->fdr_other != NULL)
			fdp->fdr_other(m_ptr, ipc_status);

		return; /* do not send a reply */
	}

	/* Call the appropriate function. */
	transid = TRNS_GET_ID(m_ptr->m_type);
	call_nr = TRNS_DEL_ID(m_ptr->m_type);

	memset(&m_out, 0, sizeof(m_out));

	if (fsdriver_mounted || call_nr == REQ_READSUPER) {
		call_nr -= FS_BASE;	/* unsigned; wrapping is intended */

		if (call_nr < NREQS && fsdriver_callvec[call_nr] != NULL)
			r = (fsdriver_callvec[call_nr])(fdp, m_ptr, &m_out);
		else
			r = ENOSYS;
	} else
		r = EINVAL;

	/* Send a reply. */
	m_out.m_type = TRNS_ADD_ID(r, transid);

	if (asyn_reply)
		r = asynsend(m_ptr->m_source, &m_out);
	else
		r = ipc_send(m_ptr->m_source, &m_out);

	if (r != OK)
		printf("fsdriver: sending reply failed (%d)\n", r);

	if (fdp->fdr_postcall != NULL)
		fdp->fdr_postcall();
}

/*
 * Terminate the file server as soon as the file system has been unmounted.
 */
void
fsdriver_terminate(void)
{

	fsdriver_running = FALSE;

	sef_cancel();
}

/*
 * Main program of any file server task.
 */
void
fsdriver_task(struct fsdriver * fdp)
{
	message mess;
	int r, ipc_status;

	fsdriver_running = TRUE;

	while (fsdriver_running || fsdriver_mounted) {
		if ((r = sef_receive_status(ANY, &mess, &ipc_status)) != OK) {
			if (r == EINTR)
				continue;	/* sef_cancel() was called */

			panic("fsdriver: sef_receive_status failed: %d", r);
		}

		fsdriver_process(fdp, &mess, ipc_status, FALSE /*asyn_reply*/);
	}
}
