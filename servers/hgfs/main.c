/* This file contains the main message loop of the HGFS file system server.
 *
 * The entry points into this file are:
 *   main		main program function
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include "optset.h"

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

FORWARD _PROTOTYPE( int get_work, (endpoint_t *who_e)			);
FORWARD _PROTOTYPE( void send_reply, (int err)				);

PRIVATE struct optset optset_table[] = {
  { "prefix",   OPT_STRING, opt.prefix,       sizeof(opt.prefix) },
  { "uid",      OPT_INT,    &opt.uid,         10                 },
  { "gid",      OPT_INT,    &opt.gid,         10                 },
  { "fmask",    OPT_INT,    &opt.file_mask,   8                  },
  { "dmask",    OPT_INT,    &opt.dir_mask,    8                  },
  { "icase",    OPT_BOOL,   &opt.case_insens, TRUE               },
  { "noicase",  OPT_BOOL,   &opt.case_insens, FALSE              },
  { NULL                                                         }
};

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( void sef_cb_signal_handler, (int signo) );

/*===========================================================================*
 *			      sef_cb_init_fresh				     *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(type, info)
int type;
sef_init_info_t *info;
{
/* Initialize this file server. Called at startup time.
 */
  message m;
  int i, r;

  /* Defaults */
  opt.prefix[0] = 0;
  opt.uid = 0;
  opt.gid = 0;
  opt.file_mask = 0755;
  opt.dir_mask = 0755;
  opt.case_insens = FALSE;

  /* If we have been given an options string, parse options from there. */
  for (i = 1; i < env_argc - 1; i++)
	if (!strcmp(env_argv[i], "-o"))
		optset_parse(optset_table, env_argv[++i]);

  /* Make sure that the given path prefix doesn't end with a slash. */
  for (i = strlen(opt.prefix); i > 0 && opt.prefix[i - 1] == '/'; i--);
  opt.prefix[i] = 0;

  /* Initialize the HGFS library. If this fails, exit immediately. */
  r = hgfs_init();
  if (r != OK && r != EAGAIN) {
	printf("HGFS: unable to initialize HGFS library (%d)\n", r);

	return r;
  }

  state.mounted = FALSE;

  /* Announce our presence to VFS. */
  m.m_type = FS_READY;

  if ((r = send(VFS_PROC_NR, &m)) != OK) {
	printf("HGFS: unable to login to VFS (%d)\n", r);

	return r;
  }

  return OK;
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  if (state.mounted) {
      dprintf(("HGFS: got SIGTERM, still mounted\n"));
  }
  else {
      dprintf(("HGFS: got SIGTERM, shutting down\n"));

      /* Pass on the cleanup request to the HGFS library. */
      hgfs_cleanup();
      exit(0);
  }
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* No live update support yet. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  sef_startup();
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main(argc, argv)
int argc;
char *argv[];
{
/* The main function of this file server. After initializing, loop forever
 * receiving one request from VFS at a time, processing it, and sending a
 * reply back to VFS.
 */
  endpoint_t who_e;
  int call_nr, err;

  env_setargs(argc, argv);
  sef_local_startup();

  for (;;) {
	call_nr = get_work(&who_e);

	if (who_e != VFS_PROC_NR) {
		continue;
	}

	if (state.mounted || call_nr == REQ_READSUPER) {
		call_nr -= VFS_BASE;

		dprintf(("HGFS: call %d\n", call_nr));

		if (call_nr >= 0 && call_nr < NREQS) {
			err = (*call_vec[call_nr])();
		} else {
			err = ENOSYS;
		}

		dprintf(("HGFS: call %d result %d\n", call_nr, err));
	}
	else err = EINVAL;

	send_reply(err);
  }

  return 0;
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE int get_work(who_e)
endpoint_t *who_e;
{
/* Receive a request message from VFS. Return the request call number.
 */
  int r;

  if ((r = sef_receive(ANY, &m_in)) != OK)
	panic("receive failed: %d", r);

  *who_e = m_in.m_source;

  return m_in.m_type;
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
PRIVATE void send_reply(err)
int err;				/* resulting error code */
{
/* Send a reply message to the requesting party, with the given error code.
 */
  int r;

  m_out.m_type = err;

  if ((r = send(m_in.m_source, &m_out)) != OK)
	printf("HGFS: send failed (%d)\n", r);
}

