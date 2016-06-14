#include "inc.h"

#define SEM_EVENTS	0x01	/* semaphore code wants process events */
static unsigned int event_mask = 0;

static int verbose = 0;

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

/*
 * Remote MIB implementation of CTL_KERN KERN_SYSVIPC KERN_SYSVIPC_INFO.  This
 * function handles all queries on the "kern.ipc.sysvipc_info" sysctl(2) node.
 */
static ssize_t
kern_ipc_info(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{

	if (call->call_namelen != 1)
		return EINVAL;

	/*
	 * Let each IPC submodule provide information through it own function.
	 * An important security note: unlike IPC_STAT and the like, access to
	 * the sysvipc_info node does not require root privileges.  That is: on
	 * NetBSD, any user can get a full listing of all IPC objects in the
	 * system.  We therefore do not perform any security check here.
	 */
	switch (call->call_name[0]) {
	case KERN_SYSVIPC_SEM_INFO:
		return get_sem_mib_info(oldp);

	case KERN_SYSVIPC_SHM_INFO:
		return get_shm_mib_info(oldp);

	default:
		return EOPNOTSUPP;
	}
}

/* The CTL_KERN KERN_SYSVIPC subtree. */
static struct rmib_node kern_ipc_table[] = {
/* 1*/	[KERN_SYSVIPC_INFO]	= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0,
				    kern_ipc_info, "sysvipc_info",
				    "System V style IPC information"),
/* 2*/	[KERN_SYSVIPC_MSG]	= RMIB_INT(RMIB_RO, 0, "sysvmsg", "System V "
				    "style message support available"),
/* 3*/	[KERN_SYSVIPC_SEM]	= RMIB_INT(RMIB_RO, 1, "sysvsem", "System V "
				    "style semaphore support available"),
/* 4*/	[KERN_SYSVIPC_SHM]	= RMIB_INT(RMIB_RO, 1, "sysvshm", "System V "
				    "style shared memory support available"),
/* 5*/	/* KERN_SYSVIPC_SHMMAX: not yet supported */
/* 6*/	/* KERN_SYSVIPC_SHMMNI: not yet supported */
/* 7*/	/* KERN_SYSVIPC_SHMSEG: not yet supported */
/* 8*/	/* KERN_SYSVIPC_SHMMAXPGS: not yet supported */
/* 9*/	/* KERN_SYSVIPC_SHMUSEPHYS: not yet supported */
	/* In addition, NetBSD has a number of dynamic nodes here. */
};

/* The CTL_KERN KERN_SYSVIPC node. */
static struct rmib_node kern_ipc_node =
    RMIB_NODE(RMIB_RO, kern_ipc_table, "ipc", "SysV IPC options");

/*
 * Initialize the IPC server.
 */
static int
sef_cb_init_fresh(int type __unused, sef_init_info_t * info __unused)
{
	const int mib[] = { CTL_KERN, KERN_SYSVIPC };
	int r;

	/*
	 * Register our own "kern.ipc" subtree with the MIB service.
	 *
	 * This call only returns local failures.  Remote failures (in the MIB
	 * service) are silently ignored.  So, we can safely panic on failure.
	 */
	if ((r = rmib_register(mib, __arraycount(mib), &kern_ipc_node)) != OK)
		panic("unable to register remote MIB tree: %d", r);

	return OK;
}

/*
 * The service has received a signal.
 */
static void
sef_cb_signal_handler(int signo)
{

	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	/*
	 * Check if there are still IPC keys around.  If not, we can safely
	 * exit immediately.  Otherwise, warn the system administrator.
	 */
	if (is_sem_nil() && is_shm_nil()) {
		rmib_deregister(&kern_ipc_node);

		sef_exit(0);
	}

	printf("IPC: exit with unclean state\n");
}

/*
 * Perform SEF initialization.
 */
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

/*
 * Update the process event subscription mask if necessary, after one of the
 * modules has changed its subscription needs.  This code is set up so that
 * support for SysV IPC message queues can be added easily later.
 */
static void
update_sub(unsigned int new_mask)
{

	/* If the old and new mask are not both zero or nonzero, update. */
	if (!event_mask != !new_mask) {
		/*
		 * Subscribe to PM process events, or unsubscribe.  While it
		 * might be tempting to implement a system that subscribes to
		 * events only from processes that are actually blocked (or
		 * using the SysV IPC facilities at all), this would result in
		 * race conditions where subscription could happen "too late"
		 * for an ongoing signal delivery, causing the affected process
		 * to deadlock.  Subscribing to events from any other call is
		 * safe however, and we exploit that to limit the kernel-level
		 * message passing overhead in the common case (which is that
		 * the IPC servier is not being used at all).  After we have
		 * unsubscribed, we may still get a few leftover events for the
		 * previous subscription, and we must properly reply to those.
		 */
		if (new_mask)
			proceventmask(PROC_EVENT_EXIT | PROC_EVENT_SIGNAL);
		else
			proceventmask(0);
	}

	event_mask = new_mask;
}

/*
 * Update the process event subscription mask for the semaphore code.
 */
void
update_sem_sub(int want_events)
{
	unsigned int new_mask;

	new_mask = event_mask & ~SEM_EVENTS;
	if (want_events)
		new_mask |= SEM_EVENTS;

	update_sub(new_mask);
}

/*
 * PM sent us a process event message.  Handle it, and reply.
 */
static void
got_proc_event(message * m)
{
	endpoint_t endpt;
	int r, has_exited;

	endpt = m->m_pm_lsys_proc_event.endpt;
	has_exited = (m->m_pm_lsys_proc_event.event == PROC_EVENT_EXIT);

	/*
	 * Currently, only semaphore handling needs to know about processes
	 * being signaled and exiting.
	 */
	if (event_mask & SEM_EVENTS)
		sem_process_event(endpt, has_exited);

	/* Echo the request as a reply back to PM. */
	m->m_type = PROC_EVENT_REPLY;
	if ((r = asynsend3(m->m_source, m, AMF_NOREPLY)) != OK)
		printf("IPC: replying to PM process event failed (%d)\n", r);
}

/*
 * The System V IPC server.
 */
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
			printf("IPC: ignoring notification from %d\n",
			    m.m_source);
			continue;
		}

		/* Process event messages from PM are handled separately. */
		if (m.m_source == PM_PROC_NR && m.m_type == PROC_EVENT) {
			got_proc_event(&m);

			continue;
		}

		/* Remote MIB messages from MIB are handled separately too. */
		if (m.m_source == MIB_PROC_NR) {
			rmib_process(&m, ipc_status);

			continue;
		}

		/* Dispatch the request. */
		call_index = (unsigned int)(m.m_type - IPC_BASE);

		if (call_index < __arraycount(call_vec) &&
		    call_vec[call_index] != NULL) {
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
