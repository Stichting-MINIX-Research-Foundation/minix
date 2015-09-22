
#include "fs.h"

static message fs_msg;
static int fs_ipc_status;
static int fs_pending;

#define PUFFS_MAX_ARGS 20

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the Minix file server. */
  return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  exitsignaled = 1;
  if (mounted)
	fs_sync();

  sef_cancel();
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
static int get_work(message *msg, int *ipc_status)
{
  int r;

  for (;;) {
	if ((r = sef_receive_status(ANY, msg, ipc_status)) != OK) {
		if (r == EINTR) /* sef_cancel from signal handler? */
			break; /* see if we can exit the main loop */
		panic("sef_receive failed: %d", r);
	}
	if (msg->m_source == VFS_PROC_NR)
		break;
	lpuffs_debug("libpuffs: unexpected source %d\n", msg->m_source);
  }

  return r;
}

int __wrap_main(int argc, char *argv[]);
int __real_main(int argc, char* argv[]);

int __wrap_main(int argc, char *argv[])
{
  int i;
  int new_argc = 0;
  static char* new_argv[PUFFS_MAX_ARGS];
  char *name;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  global_kcred.pkcr_type = PUFFCRED_TYPE_INTERNAL;

  if (argc < 3) {
	panic("Unexpected arguments, use:\
		mount -t fs /dev/ /dir [-o option1,option2]\n");
  }

  name = argv[0] + strlen(argv[0]);
  while (*name != '/' && name != argv[0])
	  name--;
  if (name != argv[0])
	  name++;
  strcpy(fs_name, name);

  new_argv[new_argc] = argv[0];
  new_argc++;

  for (i = 1; i < argc; i++) {
	if (new_argc >= PUFFS_MAX_ARGS) {
		panic("Too many arguments, change PUFFS_MAX_ARGS");
	}
	new_argv[new_argc] = argv[i];
	new_argc++;
  }

  assert(new_argc > 0);

  /* Get the mount request from VFS, so we can deal with it later. */
  (void)get_work(&fs_msg, &fs_ipc_status);
  fs_pending = TRUE;

  return __real_main(new_argc, new_argv);
}

/*
 * Receive a message unless one was already pending.  Process the message, and
 * send a reply if necessary.  Return whether puffs should keep running.
 */
int
lpuffs_pump(void)
{

	if (fs_pending == TRUE || get_work(&fs_msg, &fs_ipc_status) == OK) {
		fs_pending = FALSE;

		fsdriver_process(&puffs_table, &fs_msg, fs_ipc_status, FALSE);
	}

	return mounted || !exitsignaled;
}

/*
 * Initialize MINIX3-specific settings.
 */
void
lpuffs_init(struct puffs_usermount * pu)
{

	buildpath = pu->pu_flags & PUFFS_FLAG_BUILDPATH; /* XXX */

	LIST_INIT(&pu->pu_pnode_removed_lst);

	global_pu = pu;
}
