#include "inc.h"

/*
 * The call table for this service.
 */
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

/*
 * Initialize the IPC server.
 */
static int
sef_cb_init_fresh(int type __unused, sef_init_info_t * info __unused)
{

	return OK;
}

static void
sef_cb_signal_handler(int signo)
{

	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	/*
	 * Check if there are still IPC keys around.  If not, we can safely
	 * exit immediately.  Otherwise, warn the system administrator.
	 */
	if (is_sem_nil() && is_shm_nil())
		sef_exit(0);

	printf("IPC: exit with unclean state\n");
}

static void
sef_local_startup(void)
{

	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);

	/* Register signal callbacks. */
	sef_setcb_signal_handler(sef_cb_signal_handler);

	/* Let SEF perform startup. */
	sef_startup();
}

int
main(int argc, char ** argv)
{
	message m;
	unsigned int call_index;
	int r, ipc_status;

	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	/* The main message loop. */
	for (;;) {
		if ((r = sef_receive_status(ANY, &m, &ipc_status)) != OK)
			panic("IPC: sef_receive_status failed: %d", r);

		if (verbose)
			printf("IPC: got %d from %d\n", m.m_type, m.m_source);

		if (is_ipc_notify(ipc_status)) {
			switch (m.m_source) {
			case VM_PROC_NR:
				/*
				 * Currently, only semaphore handling needs
				 * to know about processes exiting.
				 */
				sem_process_vm_notify();
				break;
			default:
				printf("IPC: ignoring notification from %d\n",
				    m.m_source);
				break;
			}
			continue;
		}

		/* Dispatch the request. */
		call_index = (unsigned int)(m.m_type - IPC_BASE);

		if (call_index < __arraycount(call_vec) &&
		    call_vec[call_index] != NULL) {
			/*
			 * If any process does an IPC call, we have to know
			 * about it exiting.  Tell VM to watch it for us.
			 * TODO: this is not true; limit to affected processes.
			 */
			if (vm_watch_exit(m.m_source) != OK) {
				printf("IPC: watch failed on %d\n",
				    m.m_source);
			}

			r = call_vec[call_index](&m);
		} else
			r = ENOSYS;

		/* Send a reply, if needed. */
		if (r != SUSPEND) {
			if (verbose)
				printf("IPC: call result %d\n", r);

			m.m_type = r;
			/*
			 * Other fields may have been set by the handler
			 * function already.
			 */

			if ((r = ipc_sendnb(m.m_source, &m)) != OK)
				printf("IPC: send error %d\n", r);
		}

		/* XXX there must be a better way to do this! */
		update_refcount_and_destroy();
	}

	/* NOTREACHED */
	return 0;
}
