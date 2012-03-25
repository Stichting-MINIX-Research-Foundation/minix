/*	this file contains the interface of the network software with rest of
	minix. Furthermore it contains the main loop of the network task.

Copyright 1995 Philip Homburg

The valid messages and their parameters are:

from FS:
 __________________________________________________________________
|		|           |         |       |          |         |
| m_type	|   DEVICE  | PROC_NR |	COUNT |	POSITION | ADDRESS |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| DEV_OPEN 	| minor dev | proc nr | mode  |          |         |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| DEV_CLOSE 	| minor dev | proc nr |       |          |         |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| DEV_IOCTL_S	| minor dev | proc nr |       |	NWIO..	 | address |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| DEV_READ_S	| minor dev | proc nr |	count |          | address |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| DEV_WRITE_S	| minor dev | proc nr |	count |          | address |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| CANCEL	| minor dev | proc nr |       |          |         |
|_______________|___________|_________|_______|__________|_________|

from DL_ETH:
  (not documented here)
*/

#include "inet.h"

#define _MINIX_SOURCE 1

#include <fcntl.h>
#include <unistd.h>
#include <sys/svrctl.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/chardriver.h>
#include <minix/rs.h>
#include <sys/types.h>
#include <pwd.h>

#include "mq.h"
#include "qp.h"
#include "proto.h"
#include "generic/type.h"

#include "generic/arp.h"
#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/eth.h"
#include "generic/event.h"
#include "generic/ip.h"
#include "generic/psip.h"
#include "generic/rand256.h"
#include "generic/sr.h"
#include "generic/tcp.h"
#include "generic/udp.h"

THIS_FILE

#define RANDOM_DEV_NAME	"/dev/random"

endpoint_t this_proc;		/* Process number of this server. */

/* Killing Solaris */
int killer_inet= 0;

#ifdef BUF_CONSISTENCY_CHECK
extern int inet_buf_debug;
#endif

#if HZ_DYNAMIC
u32_t system_hz;
#endif

static void nw_conf(void);
static void nw_init(void);
static void ds_event(void);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);

int main(int argc, char *argv[])
{
	mq_t *mq;
	int ipc_status;
	int r;
	endpoint_t source;
	int m_type;

	/* SEF local startup. */
	sef_local_startup();

	while (TRUE)
	{
#ifdef BUF_CONSISTENCY_CHECK
		if (inet_buf_debug)
		{
			static int buf_debug_count= 0;

			if (++buf_debug_count >= inet_buf_debug)
			{
				buf_debug_count= 0;
				if (!bf_consistency_check())
					break;
			}
		}
#endif
		if (ev_head)
		{
			ev_process();
			continue;
		}
		if (clck_call_expire)
		{
			clck_expire_timers();
			continue;
		}
		mq= mq_get();
		if (!mq)
			ip_panic(("out of messages"));

		r= sef_receive_status(ANY, &mq->mq_mess, &ipc_status);
		if (r<0)
		{
			ip_panic(("unable to receive: %d", r));
		}
		reset_time();
		source= mq->mq_mess.m_source;
		m_type= mq->mq_mess.m_type;
		if (source == VFS_PROC_NR)
		{
			sr_rec(mq);
		}
		else if (is_ipc_notify(ipc_status))
		{
			if (source == CLOCK)
			{
				clck_tick(&mq->mq_mess);
				mq_free(mq);
			} 
			else if (source == PM_PROC_NR)
			{
				/* signaled */ 
				/* probably SIGTERM */
				mq_free(mq);
			} 
			else if (source == DS_PROC_NR)
			{
				/* DS notifies us of an event. */
				ds_event();
				mq_free(mq);
			}
			else
			{
				printf("inet: got unexpected notify from %d\n",
					mq->mq_mess.m_source);
				mq_free(mq);
			}
		}
		else if (m_type == DL_CONF_REPLY || m_type == DL_TASK_REPLY ||
			m_type == DL_STAT_REPLY)
		{
			eth_rec(&mq->mq_mess);
			mq_free(mq);
		}
		else
		{
			printf("inet: got bad message type 0x%x from %d\n",
				mq->mq_mess.m_type, mq->mq_mess.m_source);
			mq_free(mq);
		}
	}
	ip_panic(("task is not allowed to terminate"));
	return 1;
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

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the inet server. */
	int r;
	int timerand, fd;
	u8_t randbits[32];
	struct timeval tv;
	struct passwd *pw;

#if DEBUG
	printf("Starting inet...\n");
	printf("%s\n", version);
#endif

#if HZ_DYNAMIC
	system_hz = sys_hz();
#endif

	/* Read configuration. */
	nw_conf();

	/* Get a random number */
	timerand= 1;
	fd= open(RANDOM_DEV_NAME, O_RDONLY | O_NONBLOCK);
	if (fd != -1)
	{
		r= read(fd, randbits, sizeof(randbits));
		if (r == sizeof(randbits))
			timerand= 0;
		else
		{
			printf("inet: unable to read random data from %s: %s\n",
				RANDOM_DEV_NAME, r == -1 ? strerror(errno) :
				r == 0 ? "EOF" : "not enough data");
		}
		close(fd);
	}
	else
	{
		printf("inet: unable to open random device %s: %s\n",
			RANDOM_DEV_NAME, strerror(errno));
	}
	if (timerand)
	{
		printf("inet: using current time for random-number seed\n");
		r= gettimeofday(&tv, NULL);
		if (r == -1)
		{
			printf("sysutime failed: %s\n", strerror(errno));
			exit(1);
		}
		memcpy(randbits, &tv, sizeof(tv));
	}
	init_rand256(randbits);

	/* Our new identity as a server. */
	this_proc= info->endpoint;

#ifdef BUF_CONSISTENCY_CHECK
	inet_buf_debug= (getenv("inetbufdebug") && 
		(strcmp(getenv("inetbufdebug"), "on") == 0));
	inet_buf_debug= 100;
	if (inet_buf_debug)
	{
		ip_warning(( "buffer consistency check enabled" ));
	}
#endif

	if (getenv("killerinet"))
	{
		ip_warning(( "killer inet active" ));
		killer_inet= 1;
	}

	nw_init();

	/* Subscribe to driver events for network drivers. */
	r = ds_subscribe("drv\\.net\\..*", DSF_INITIAL | DSF_OVERWRITE);
	if(r != OK) {
		ip_panic(("inet: can't subscribe to driver events"));
	}

	/* Drop root privileges */
	if ((pw = getpwnam(SERVICE_LOGIN)) == NULL) {
		printf("inet: unable to retrieve uid of SERVICE_LOGIN, "
			"still running as root");
	} else if (setuid(pw->pw_uid) != 0) {
		ip_panic(("inet: unable to drop privileges"));
	}

	/* Announce we are up. INET announces its presence to VFS just like
	 * any other character driver.
	 */
	chardriver_announce();

	return(OK);
}

static void nw_conf()
{
	read_conf();
	eth_prep();
	arp_prep();
	psip_prep();
	ip_prep();
	tcp_prep();
	udp_prep();
}

static void nw_init()
{
	mq_init();
	bf_init();
	clck_init();
	sr_init();
	qp_init();
	eth_init();
	arp_init();
	psip_init();
	ip_init();
	tcp_init();
	udp_init();
}

/*===========================================================================*
 *				 ds_event				     *
 *===========================================================================*/
static void ds_event()
{
	char key[DS_MAX_KEYLEN];
	char *driver_prefix = "drv.net.";
	char *label;
	u32_t value;
	int type;
	endpoint_t owner_endpoint;
	int r;
	int prefix_len;

	prefix_len = strlen(driver_prefix);

	/* We may get one notification for multiple updates from DS. Get events
	 * and owners from DS, until DS tells us that there are no more.
	 */
	while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {
		r = ds_retrieve_u32(key, &value);
		if(r != OK) {
			printf("inet: ds_event: ds_retrieve_u32 failed\n");
			return;
		}

		/* Only check for network driver up events. */
		if(strncmp(key, driver_prefix, prefix_len)
		   || value != DS_DRIVER_UP) {
			return;
		}

		/* The driver label comes after the prefix. */
		label = key + prefix_len;

		/* A driver is (re)started. */
		eth_check_driver(label, owner_endpoint);
	}

	if(r != ENOENT)
		printf("inet: ds_event: ds_check failed: %d\n", r);
}

void panic0(file, line)
char *file;
int line;
{
	printf("panic at %s, %d: ", file, line);
}

void inet_panic()
{
	printf("\ninet stacktrace: ");
	util_stacktrace();
	(panic)("aborted due to a panic");
	for(;;);
}

#if !NDEBUG
void bad_assertion(file, line, what)
char *file;
int line;
char *what;
{
	panic0(file, line);
	printf("assertion \"%s\" failed", what);
	panic();
}


void bad_compare(file, line, lhs, what, rhs)
char *file;
int line;
int lhs;
char *what;
int rhs;
{
	panic0(file, line);
	printf("compare (%d) %s (%d) failed", lhs, what, rhs);
	panic();
}
#endif /* !NDEBUG */

/*
 * $PchId: inet.c,v 1.23 2005/06/28 14:27:22 philip Exp $
 */
