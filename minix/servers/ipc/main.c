#include "inc.h"

endpoint_t who_e;
static unsigned int call_type;

#define CALL(n) [((n) - IPC_BASE)]
static int (* const call_vec[])(message *) = {
	CALL(IPC_SHMGET)	= do_shmget,
	CALL(IPC_SHMAT)		= do_shmat,
	CALL(IPC_SHMDT)		= do_shmdt,
	CALL(IPC_SHMCTL)	= do_shmctl,
	CALL(IPC_SEMGET)	= do_semget,
	CALL(IPC_SEMCTL)	= do_semctl,
	CALL(IPC_SEMOP)		= do_semop,
};

static int verbose = 0;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

int main(int argc, char *argv[])
{
	message m;
	unsigned int ipc_number;
	int r;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		if ((r = sef_receive(ANY, &m)) != OK)
			printf("sef_receive failed %d.\n", r);
		who_e = m.m_source;
		call_type = m.m_type;

		if(verbose)
			printf("IPC: get %d from %d\n", call_type, who_e);

		if (call_type & NOTIFY_MESSAGE) {
			switch (who_e) {
			case VM_PROC_NR:
				/* currently, only semaphore needs such information. */
				sem_process_vm_notify();
				break;
			default:
				printf("IPC: ignoring notify() from %d\n",
					who_e);
				break;
			}
			continue;
		}

		ipc_number = (unsigned int)(call_type - IPC_BASE);

		/* dispatch message */
		if (ipc_number < __arraycount(call_vec) &&
		    call_vec[ipc_number] != NULL) {
			/* If any process does an IPC call,
			 * we have to know about it exiting.
			 * Tell VM to watch it for us.
			 */
			if(vm_watch_exit(m.m_source) != OK) {
				printf("IPC: watch failed on %d\n", m.m_source);
			}

			r = call_vec[ipc_number](&m);

			/*
			 * The handler of the IPC call did not
			 * post a reply.
			 */
			if (r != SUSPEND) {
				m.m_type = r;

				if(verbose && r != OK)
					printf("IPC: error for %d: %d\n",
						call_type, r);

				if ((r = ipc_sendnb(who_e, &m)) != OK)
					printf("IPC send error %d.\n", r);
			}
		} else {
			/* warn and then ignore */
			printf("IPC unknown call type: %d from %d.\n",
				call_type, who_e);
		}
		update_refcount_and_destroy();
	}

	/* no way to get here */
	return -1;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the ipc server. */

  if(verbose)
      printf("IPC: self: %d\n", sef_self());

  return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* Checkout if there are still IPC keys. Inform the user in that case. */
  if (is_sem_nil() && is_shm_nil())
	sef_exit(0);

  printf("IPC: exit with un-clean states.\n");
}
