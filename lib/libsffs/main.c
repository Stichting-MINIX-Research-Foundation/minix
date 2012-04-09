/* This file contains the SFFS initialization code and message loop.
 *
 * The entry points into this file are:
 *   sffs_init		initialization
 *   sffs_signal	signal handler
 *   sffs_loop		main message loop
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				sffs_init				     *
 *===========================================================================*/
int sffs_init(char *name, const struct sffs_table *table,
  struct sffs_params *params)
{
/* Initialize this file server. Called at startup time.
 */
  int i;

  /* Make sure that the given path prefix doesn't end with a slash. */
  i = strlen(params->p_prefix);
  while (i > 0 && params->p_prefix[i - 1] == '/') i--;
  params->p_prefix[i] = 0;

  state.s_mounted = FALSE;
  state.s_signaled = FALSE;

  sffs_name = name;
  sffs_table = table;
  sffs_params = params;

  return OK;
}

/*===========================================================================*
 *				sffs_signal				     *
 *===========================================================================*/
void sffs_signal(int signo)
{

  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* We can now terminate if we have also been unmounted. */
  state.s_signaled = TRUE;

  if (state.s_mounted) {
	dprintf(("%s: got SIGTERM, still mounted\n", sffs_name));
  } else {
	dprintf(("%s: got SIGTERM, shutting down\n", sffs_name));

	/* Break out of the main loop, giving the main program the chance to
	 * perform further cleanup. This causes sef_receive() to return with
	 * an EINTR error code.
	 */
	sef_cancel();
  }
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
static int get_work(who_e)
endpoint_t *who_e;
{
/* Receive a request message from VFS. Return TRUE if a new message is ready
 * to be processed, or FALSE if sef_stop() was called from the signal handler.
 */
  int r;

  if ((r = sef_receive(ANY, &m_in)) != OK) {
	if (r != EINTR)
		panic("receive failed: %d", r);

	return FALSE;
  }

  *who_e = m_in.m_source;
  return TRUE;
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
static void send_reply(err, transid)
int err;				/* resulting error code */
int transid;
{
/* Send a reply message to the requesting party, with the given error code.
 */
  int r;

  m_out.m_type = err;
  if (IS_VFS_FS_TRANSID(transid)) {
	/* If a transaction ID was set, reset it */
	m_out.m_type = TRNS_ADD_ID(m_out.m_type, transid);
  }
  if ((r = send(m_in.m_source, &m_out)) != OK)
	printf("%s: send failed (%d)\n", sffs_name, r);
}

/*===========================================================================*
 *				sffs_loop				     *
 *===========================================================================*/
void sffs_loop(void)
{
/* The main function of this file server. After initializing, loop, receiving
 * one request from VFS at a time, processing it, and sending a reply back to
 * VFS. Termination occurs when we both have been unmounted and have received
 * a termination signal.
 */
  endpoint_t who_e;
  int call_nr, err, transid;

  while (state.s_mounted || !state.s_signaled) {
	if (!get_work(&who_e))
		continue;	/* Recheck running conditions */

	transid = TRNS_GET_ID(m_in.m_type);
	m_in.m_type = TRNS_DEL_ID(m_in.m_type);
	if (m_in.m_type == 0) {
		assert(!IS_VFS_FS_TRANSID(transid));
		m_in.m_type = transid;		/* Backwards compat. */
		transid = 0;
	} else
		assert(IS_VFS_FS_TRANSID(transid));

	call_nr = m_in.m_type;
	if (who_e != VFS_PROC_NR) {
		continue;
	}

	if (state.s_mounted || call_nr == REQ_READSUPER) {
		call_nr -= VFS_BASE;

		dprintf(("%s: call %d\n", sffs_name, call_nr));

		if (call_nr >= 0 && call_nr < NREQS) {
			err = (*call_vec[call_nr])();
		} else {
			err = ENOSYS;
		}

		dprintf(("%s: call %d result %d\n", sffs_name, call_nr, err));
	}
	else err = EINVAL;

	send_reply(err, transid);
  }
}
