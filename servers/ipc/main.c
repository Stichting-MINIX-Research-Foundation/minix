#include "inc.h"

PUBLIC int identifier = 0x1234;
PUBLIC endpoint_t who_e;
PUBLIC int call_type;
PUBLIC endpoint_t SELF_E;

struct {
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

int verbose = 0;

PUBLIC int main(int argc, char *argv[])
{
	message m;

	SELF_E = getprocnr();

	if(verbose)
		printf("IPC: self: %d\n", SELF_E);

	while (TRUE) {
		int r;
		int i;

		if ((r = receive(ANY, &m)) != OK)
			printf("IPC receive error %d.\n", r);
		who_e = m.m_source;
		call_type = m.m_type;

		if(verbose)
			printf("IPC: get %d from %d\n", call_type, who_e);

		if (call_type & NOTIFY_MESSAGE) {
			switch (who_e) {
			case PM_PROC_NR:
				/* PM sends a notify() on shutdown,
				 * checkout if there are still IPC keys,
				 * give warning messages.
				 */
				if (!is_sem_nil() || !is_shm_nil())
					printf("IPC: exit with un-clean states.\n");
				break;
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

