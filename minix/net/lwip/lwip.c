/* LWIP service - lwip.c - main program and dispatch code */

#include "lwip.h"
#include "tcpisn.h"
#include "mcast.h"
#include "ethif.h"
#include "rtsock.h"
#include "route.h"
#include "bpfdev.h"

#include "lwip/init.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "arch/cc.h"

static int running, recheck_timer;
static minix_timer_t lwip_timer;

static void expire_lwip_timer(int);

/*
 * Return the system uptime in milliseconds.  Also remember that lwIP retrieved
 * the system uptime during this call, so that we know to check for timer
 * updates at the end of the current iteration of the message loop.
 */
uint32_t
sys_now(void)
{

	recheck_timer = TRUE;

	/* TODO: avoid 64-bit arithmetic if possible. */
	return (uint32_t)(((uint64_t)getticks() * 1000) / sys_hz());
}

/*
 * Check if and when lwIP has its next timeout, and set or cancel our timer
 * accordingly.
 */
static void
set_lwip_timer(void)
{
	uint32_t next_timeout;
	clock_t ticks;

	/* Ask lwIP when the next alarm is supposed to go off, if any. */
	next_timeout = sys_timeouts_sleeptime();

	/*
	 * Set or update the lwIP timer.  We rely on set_timer() asking the
	 * kernel for an alarm only if the timeout is different from the one we
	 * gave it last time (if at all).  However, due to conversions between
	 * absolute and relative times, and the fact that we cannot guarantee
	 * that the uptime itself does not change while executing these
	 * routines, set_timer() will sometimes be issuing a kernel call even
	 * if the alarm has not changed.  Not a huge deal, but fixing this will
	 * require a different interface to lwIP and/or the timers library.
	 */
	if (next_timeout != (uint32_t)-1) {
		/*
		 * Round up the next timeout (which is in milliseconds) to the
		 * number of clock ticks to add to the current time.  Avoid any
		 * potential for overflows, no matter how unrealistic..
		 */
		if (next_timeout > TMRDIFF_MAX / sys_hz())
			ticks = TMRDIFF_MAX;
		else
			ticks = (next_timeout * sys_hz() + 999) / 1000;

		set_timer(&lwip_timer, ticks, expire_lwip_timer, 0 /*unused*/);
	} else
		cancel_timer(&lwip_timer);	/* not really needed.. */
}

/*
 * The timer for lwIP timeouts has gone off.  Check timeouts, and possibly set
 * a new timer.
 */
static void
expire_lwip_timer(int arg __unused)
{

	/* Let lwIP do its work. */
	sys_check_timeouts();

	/*
	 * See if we have to update our timer for the next lwIP timer.  Doing
	 * this here, rather than from the main loop, avoids one kernel call.
	 */
	set_lwip_timer();

	recheck_timer = FALSE;
}

/*
 * Check whether we should adjust our local timer based on a change in the next
 * lwIP timeout.
 */
static void
check_lwip_timer(void)
{

	/*
	 * We make the assumption that whenever lwIP starts a timer, it will
	 * need to retrieve the current time.  Thus, whenever sys_now() is
	 * called, we set the 'recheck_timer' flag.  Here, we check whether to
	 * (re)set our lwIP timer only if the flag is set.  As a result, we do
	 * not have to mess with timers for literally every incoming message.
	 *
	 * When lwIP stops a timer, it does not call sys_now(), and thus, we
	 * may miss such updates.  However, timers being stopped should be rare
	 * and getting too many alarm messages is not a big deal.
	 */
	if (!recheck_timer)
		return;

	set_lwip_timer();

	/* Reset the flag for the next message loop iteration. */
	recheck_timer = FALSE;
}

/*
 * Return a random number, for use by lwIP.
 */
uint32_t
lwip_hook_rand(void)
{

	/*
	 * The current known uses of this hook are for selection of initial
	 * TCP/UDP port numbers and for multicast-related timer randomness.
	 * The former case exists only to avoid picking the same starting port
	 * numbers after a reboot.  After that, simple sequential iteration of
	 * the port numbers is used.  The latter case varies the response time
	 * for sending multicast messages.  Thus, none of the current uses of
	 * this function require proper randomness, and so we use the simplest
	 * approach, with time-based initialization to cover the reboot case.
	 * The sequential port number selection could be improved upon, but
	 * such an extension would probably bypass this hook anyway.
	 */
	return lrand48();
}

/*
 * Create a new socket, with the given domain, type, and protocol, for the user
 * process identified by 'user_endpt'.  On success, return the new socket's
 * identifier, with the libsockevent socket stored in 'sock' and an operations
 * table stored in 'ops'.  On failure, return a negative error code.
 */
static sockid_t
alloc_socket(int domain, int type, int protocol, endpoint_t user_endpt,
	struct sock ** sock, const struct sockevent_ops **ops)
{

	switch (domain) {
	case PF_INET:
#ifdef INET6
	case PF_INET6:
#endif /* INET6 */
		switch (type) {
		case SOCK_STREAM:
			return tcpsock_socket(domain, protocol, sock, ops);

		case SOCK_DGRAM:
			return udpsock_socket(domain, protocol, sock, ops);

		case SOCK_RAW:
			if (!util_is_root(user_endpt))
				return EACCES;

			return rawsock_socket(domain, protocol, sock, ops);

		default:
			return EPROTOTYPE;
		}

	case PF_ROUTE:
		return rtsock_socket(type, protocol, sock, ops);

	case PF_LINK:
		return lnksock_socket(type, protocol, sock, ops);

	default:
		/* This means that the service has been misconfigured. */
		printf("socket() with unsupported domain %d\n", domain);

		return EAFNOSUPPORT;
	}
}

/*
 * Initialize the service.
 */
static int
init(int type __unused, sef_init_info_t * init __unused)
{

	/*
	 * Initialize the random number seed.  See the lwip_hook_rand() comment
	 * on why this weak random number source is currently sufficient.
	 */
	srand48(clock_time(NULL));

	/* Initialize the lwIP library. */
	lwip_init();

	/* Initialize the socket events library. */
	sockevent_init(alloc_socket);

	/* Initialize various helper modules. */
	mempool_init();
	tcpisn_init();
	mcast_init();

	/* Initialize the high-level socket modules. */
	ipsock_init();
	tcpsock_init();
	udpsock_init();
	rawsock_init();

	/* Initialize the various network interface modules. */
	ifdev_init();
	loopif_init();
	ethif_init();

	/* Initialize the network device driver module. */
	ndev_init();

	/* Initialize the low-level socket modules. */
	rtsock_init();
	lnksock_init();

	/* Initialize the routing module. */
	route_init();

	/* Initialize other device modules. */
	bpfdev_init();

	/*
	 * Initialize the MIB module, after all other modules have registered
	 * their subtrees with this module.
	 */
	mibtree_init();

	/*
	 * After everything else has been initialized, set up the default
	 * configuration - in particular, a loopback interface.
	 */
	ifconf_init();

	/*
	 * Initialize the master timer for all the lwIP timers.  Just in case
	 * lwIP starts a timer right away, perform a first check upon entry of
	 * the message loop.
	 */
	init_timer(&lwip_timer);

	recheck_timer = TRUE;

	running = TRUE;

	return OK;
}

/*
 * Perform initialization using the System Event Framework (SEF).
 */
static void
startup(void)
{

	sef_setcb_init_fresh(init);
	/*
	 * This service requires stateless restarts, in that several parts of
	 * the system (including VFS and drivers) expect that if restarted,
	 * this service comes back up with a new endpoint.  Therefore, do not
	 * set a _restart callback here.
	 *
	 * TODO: support for live update.
	 *
	 * TODO: support for immediate shutdown if no sockets are in use, as
	 * also done by UDS.  For now, we never shut down immediately, giving
	 * other processes the opportunity to close sockets on system shutdown.
	 */

	sef_startup();
}

/*
 * The lwIP-based TCP/IP sockets driver.
 */
int
main(void)
{
	message m;
	int r, ipc_status;

	startup();

	while (running) {
		/*
		 * For various reasons, the loopback interface does not pass
		 * packets back into the stack right away.  Instead, it queues
		 * them up for later processing.  We do that processing here.
		 */
		ifdev_poll();

		/*
		 * Unfortunately, lwIP does not tell us when it starts or stops
		 * timers.  This means that we have to check ourselves every
		 * time we have called into lwIP.  For simplicity, we perform
		 * the check here.
		 */
		check_lwip_timer();

		if ((r = sef_receive_status(ANY, &m, &ipc_status)) != OK) {
			if (r == EINTR)
				continue;	/* sef_cancel() was called */

			panic("sef_receive_status failed: %d", r);
		}

		/* Process the received message. */
		if (is_ipc_notify(ipc_status)) {
			switch (m.m_source) {
			case CLOCK:
				expire_timers(m.m_notify.timestamp);

				break;

			case DS_PROC_NR:
				/* Network drivers went up and/or down. */
				ndev_check();

				break;

			default:
				printf("unexpected notify from %d\n",
				    m.m_source);
			}

			continue;
		}

		switch (m.m_source) {
		case MIB_PROC_NR:
			rmib_process(&m, ipc_status);

			break;

		case VFS_PROC_NR:
			/* Is this a socket device request? */
			if (IS_SDEV_RQ(m.m_type)) {
				sockevent_process(&m, ipc_status);

				break;
			}

			/* Is this a character (or block) device request? */
			if (IS_CDEV_RQ(m.m_type) || IS_BDEV_RQ(m.m_type)) {
				bpfdev_process(&m, ipc_status);

				break;
			}

			/* FALLTHROUGH */
		default:
			/* Is this a network device driver response? */
			if (IS_NDEV_RS(m.m_type)) {
				ndev_process(&m, ipc_status);

				break;
			}

			printf("unexpected message %d from %d\n",
			    m.m_type, m.m_source);
		}
	}

	return 0;
}
