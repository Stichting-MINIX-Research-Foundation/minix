#include "inc.h"

PUBLIC int identifier = 0x1234;
PUBLIC endpoint_t who_e;
PUBLIC int call_type;
PUBLIC endpoint_t SELF_E;

PRIVATE struct {
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

PRIVATE int verbose = 0;

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( void sef_cb_signal_handler, (int signo) );

PUBLIC int main(int argc, char *argv[])
{
	message m;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	while (TRUE) {
		int r;
		int i;

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

		/* dispatch messages */
		for (i = 0; i < SIZE(ipc_calls); i++) {
			if (ipc_calls[i].type == call_type) {
				int result;

				result = ipc_calls[i].func(&m);

				if (ipc_calls[i].reply)
					break;

				m.m_type = result;

				if(verbose && result != OK)
					printf("IPC: error for %d: %d\n",
						call_type, result);

				if ((r = sendnb(who_e, &m)) != OK)
					printf("IPC send error %d.\n", r);
				break;
			}
		}

		if (i == SIZE(ipc_calls)) {
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
PRIVATE void sef_local_startup()
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
PRIVATE int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the ipc server. */

  SELF_E = getprocnr();

  if(verbose)
      printf("IPC: self: %d\n", SELF_E);

  return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* Checkout if there are still IPC keys. Inform the user in that case. */
  if (!is_sem_nil() || !is_shm_nil())
      printf("IPC: exit with un-clean states.\n");
}

