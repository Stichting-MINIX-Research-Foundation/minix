#include "inc.h"

int identifier = 0x1234;
endpoint_t who_e;
int call_type;

static struct {
	int type;
	int (*func)(message *);
	int reply;	/* whether the reply action is passed through */
} ipc_calls[] = {
	{ IPC_SHMGET,	do_shmget,	0 },
	{ IPC_SHMAT,	do_shmat,	0 },
	{ IPC_SHMDT,	do_shmdt,	0 },
	{ IPC_SHMCTL,	do_shmctl,	0 },
	{ IPC_SEMGET,	do_semget,	0 },
	{ IPC_SEMCTL,	do_semctl,	0 },
	{ IPC_SEMOP,	do_semop,	1 },
};

#define SIZE(a) (sizeof(a)/sizeof(a[0]))

static int verbose = 0;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

int main(int argc, char *argv[])
{
	message m;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		int r;
		int ipc_number;

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

		/*
		 * The ipc number in the table can be obtained
		 * with a simple equation because the values of
		 * IPC system calls are consecutive and begin
		 * at ( IPC_BASE + 1 )
		 */

		ipc_number = call_type - (IPC_BASE + 1);

		/* dispatch message */
		if (ipc_number >= 0 && ipc_number < SIZE(ipc_calls)) {
			int result;

			if (ipc_calls[ipc_number].type != call_type)
				panic("IPC: call table order mismatch");

			/* If any process does an IPC call,
			 * we have to know about it exiting.
			 * Tell VM to watch it for us.
			 */
			if(vm_watch_exit(m.m_source) != OK) {
				printf("IPC: watch failed on %d\n", m.m_source);
			}

			result = ipc_calls[ipc_number].func(&m);

			/*
			 * The handler of the IPC call did not
			 * post a reply.
			 */
			if (!ipc_calls[ipc_number].reply) {

				m.m_type = result;

				if(verbose && result != OK)
					printf("IPC: error for %d: %d\n",
						call_type, result);

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
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* No live update support for now. */

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
  if (!is_sem_nil() || !is_shm_nil())
      printf("IPC: exit with un-clean states.\n");
}

