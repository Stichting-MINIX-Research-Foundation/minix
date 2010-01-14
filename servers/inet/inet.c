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
| NW_OPEN 	| minor dev | proc nr | mode  |          |         |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| NW_CLOSE 	| minor dev | proc nr |       |          |         |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| NW_IOCTL	| minor dev | proc nr |       |	NWIO..	 | address |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| NW_READ	| minor dev | proc nr |	count |          | address |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| NW_WRITE	| minor dev | proc nr |	count |          | address |
|_______________|___________|_________|_______|__________|_________|
|		|           |         |       |          |         |
| NW_CANCEL	| minor dev | proc nr |       |          |         |
|_______________|___________|_________|_______|__________|_________|

from DL_ETH:
 _______________________________________________________________________
|		|           |         |          |            |         |
| m_type	|  DL_PORT  | DL_PROC |	DL_COUNT |  DL_STAT   | DL_TIME |
|_______________|___________|_________|__________|____________|_________|
|		|           |         |          |            |         |
| DL_CONF_REPLY	| minor dev | proc nr | rd_count |  0  | stat |  time   |
|_______________|___________|_________|__________|____________|_________|
|		|           |         |          |            |         |
| DL_TASK_REPLY	| minor dev | proc nr | rd_count | err | stat |  time   |
|_______________|___________|_________|__________|____________|_________|
*/

#include "inet.h"

#define _MINIX_SOURCE 1

#include <fcntl.h>
#include <unistd.h>
#include <sys/svrctl.h>
#include <minix/ds.h>
#include <minix/endpoint.h>

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

int this_proc;		/* Process number of this server. */

/* Killing Solaris */
int killer_inet= 0;

#ifdef BUF_CONSISTENCY_CHECK
extern int inet_buf_debug;
#endif

#if HZ_DYNAMIC
u32_t system_hz;
#endif

_PROTOTYPE( void main, (void) );

FORWARD _PROTOTYPE( void nw_conf, (void) );
FORWARD _PROTOTYPE( void nw_init, (void) );

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

PUBLIC void main()
{
	mq_t *mq;
	int r;
	int source, m_type;

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

		r= sef_receive(ANY, &mq->mq_mess);
		if (r<0)
		{
			ip_panic(("unable to receive: %d", r));
		}
		reset_time();
		source= mq->mq_mess.m_source;
		m_type= mq->mq_mess.m_type;
		if (source == FS_PROC_NR)
		{
			sr_rec(mq);
		}
		else if (is_notify(m_type))
		{
			if (_ENDPOINT_P(source) == CLOCK)
			{
				clck_tick(&mq->mq_mess);
				mq_free(mq);
			} 
			else if (_ENDPOINT_P(source) == PM_PROC_NR)
			{
				/* signaled */ 
				/* probably SIGTERM */
				mq_free(mq);
			} 
			else
			{
				/* A driver is (re)started. */
				eth_check_drivers(&mq->mq_mess);
				mq_free(mq);
			}
		}
		else if (m_type == DL_CONF_REPLY || m_type == DL_TASK_REPLY ||
			m_type == DL_NAME_REPLY || m_type == DL_STAT_REPLY)
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
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_restart_fail);

  /* No live update support for now. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the inet server. */
	int r;
	int timerand, fd;
	u32_t tasknr;
	struct fssignon device;
	u8_t randbits[32];
	struct timeval tv;

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
	r= ds_retrieve_label_num("inet", &tasknr);
	if (r != OK)
		ip_panic(("inet: ds_retrieve_label_num failed for 'inet': %d", r));
	this_proc= tasknr;

	/* Register the device group. */
	device.dev= ip_dev;
	device.style= STYLE_CLONE;
	if (svrctl(FSSIGNON, (void *) &device) == -1) {
		printf("inet: error %d on registering ethernet devices\n",
			errno);
		pause();
	}

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

	return(OK);
}

PRIVATE void nw_conf()
{
	read_conf();
	eth_prep();
	arp_prep();
	psip_prep();
	ip_prep();
	tcp_prep();
	udp_prep();
}

PRIVATE void nw_init()
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

PUBLIC void panic0(file, line)
char *file;
int line;
{
	printf("panic at %s, %d: ", file, line);
}

PUBLIC void inet_panic()
{
	printf("\ninet stacktrace: ");
	util_stacktrace();
	(panic)("INET","aborted due to a panic",NO_NUM);
	for(;;);
}

#if !NDEBUG
PUBLIC void bad_assertion(file, line, what)
char *file;
int line;
char *what;
{
	panic0(file, line);
	printf("assertion \"%s\" failed", what);
	panic();
}


PUBLIC void bad_compare(file, line, lhs, what, rhs)
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
