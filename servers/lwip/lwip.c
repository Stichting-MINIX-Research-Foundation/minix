#include <unistd.h>
#include <timers.h>
#include <sys/svrctl.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <errno.h>
#include <minix/sef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <minix/chardriver.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/timers.h>
#include <minix/netsock.h>

#include "proto.h"

#include <lwip/mem.h>
#include <lwip/pbuf.h>
#include <lwip/stats.h>
#include <lwip/netif.h>
#include <netif/etharp.h>
#include <lwip/tcp_impl.h>

endpoint_t lwip_ep;

static timer_t tcp_ftmr, tcp_stmr, arp_tmr;
static int arp_ticks, tcp_fticks, tcp_sticks;

static struct netif * netif_lo;

extern struct sock_ops sock_udp_ops;
extern struct sock_ops sock_tcp_ops;
extern struct sock_ops sock_raw_ip_ops;

void sys_init(void)
{
}

static void arp_watchdog(__unused timer_t *tp)
{
	etharp_tmr();
	set_timer(&arp_tmr, arp_ticks, arp_watchdog, 0);
}

static void tcp_fwatchdog(__unused timer_t *tp)
{
	tcp_fasttmr();
	set_timer(&tcp_ftmr, tcp_fticks, tcp_fwatchdog, 0);
}

static void tcp_swatchdog(__unused timer_t *tp)
{
	tcp_slowtmr();
	set_timer(&tcp_ftmr, tcp_sticks, tcp_swatchdog, 0);
}

static int sef_cb_init_fresh(__unused int type, __unused sef_init_info_t *info)
{
	int err;
	unsigned hz;

	char my_name[16];
	int my_priv;

	err = sys_whoami(&lwip_ep, my_name, sizeof(my_name), &my_priv);
	if (err != OK)
		panic("Cannot get own endpoint");

	nic_init_all();
	inet_read_conf();

	/* init lwip library */
	stats_init();
	sys_init();
	mem_init();
	memp_init();
	pbuf_init();

	hz = sys_hz();

	arp_ticks = ARP_TMR_INTERVAL / (1000 / hz);
	tcp_fticks = TCP_FAST_INTERVAL / (1000 / hz);
	tcp_sticks = TCP_SLOW_INTERVAL / (1000 / hz);

	etharp_init();
	
	set_timer(&arp_tmr, arp_ticks, arp_watchdog, 0);
	set_timer(&tcp_ftmr, tcp_fticks, tcp_fwatchdog, 0);
	set_timer(&tcp_stmr, tcp_sticks, tcp_swatchdog, 0);
	
	netif_init();
	netif_lo = netif_find("lo0");

	/* Read configuration. */
#if 0
	nw_conf();

	/* Get a random number */
	timerand= 1;
	fd = open(RANDOM_DEV_NAME, O_RDONLY | O_NONBLOCK);
	if (fd != -1)
	{
		err= read(fd, randbits, sizeof(randbits));
		if (err == sizeof(randbits))
			timerand= 0;
		else
		{
			printf("inet: unable to read random data from %s: %s\n",
				RANDOM_DEV_NAME, err == -1 ? strerror(errno) :
				err == 0 ? "EOF" : "not enough data");
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
		err= gettimeofday(&tv, NULL);
		if (err == -1)
		{
			printf("sysutime failed: %s\n", strerror(errno));
			exit(1);
		}
		memcpy(randbits, &tv, sizeof(tv));
	}
	init_rand256(randbits);
#endif

	/* Subscribe to driver events for network drivers. */
	if ((err = ds_subscribe("drv\\.net\\..*",
					DSF_INITIAL | DSF_OVERWRITE)) != OK)
		panic(("inet: can't subscribe to driver events"));

	/* Announce we are up. LWIP announces its presence to VFS just like
	 * any other character driver.
	 */
	chardriver_announce();

	return(OK);
}

static void sef_local_startup()
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);

	/* No live update support for now. */

	/* Let SEF perform startup. */
	sef_startup();
}

static void ds_event(void)
{
	char key[DS_MAX_KEYLEN];
	char *driver_prefix = "drv.net.";
	char *label;
	u32_t value;
	int type;
	endpoint_t owner_endpoint;
	int r;

	/* We may get one notification for multiple updates from DS. Get events
	 * and owners from DS, until DS tells us that there are no more.
	 */
	while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {
		r = ds_retrieve_u32(key, &value);
		if(r != OK) {
			printf("LWIP : ds_event: ds_retrieve_u32 failed\n");
			return;
		}

		/* Only check for network driver up events. */
		if(strncmp(key, driver_prefix, sizeof(driver_prefix))
				|| value != DS_DRIVER_UP)
			return;

		/* The driver label comes after the prefix. */
		label = key + strlen(driver_prefix);

		/* A driver is (re)started. */
		driver_up(label, owner_endpoint);
	}

	if(r != ENOENT)
		printf("LWIP : ds_event: ds_check failed: %d\n", r);
}

static void netif_poll_lo(void)
{
	if (netif_lo == NULL)
		return;

	while (netif_lo->loop_first)
		netif_poll(netif_lo);
}

void socket_open(message * m)
{
        struct sock_ops * ops;
        struct socket * sock;
        int ret = OK;

        switch (m->DEVICE) {
        case SOCK_TYPE_TCP:
                ops = &sock_tcp_ops;
                break;
        case SOCK_TYPE_UDP:
                ops = &sock_udp_ops;
                break;
        case SOCK_TYPE_IP:
                ops = &sock_raw_ip_ops;
                break;
        default:
                if (m->DEVICE - SOCK_TYPES  < MAX_DEVS) {
                        m->DEVICE -= SOCK_TYPES;
                        nic_open(m);
                        return;
                }
                printf("LWIP unknown socket type %d\n", m->DEVICE);
                send_reply_open(m, EINVAL);
                return;
        }

        sock = get_unused_sock();
        if (!sock) {
                printf("LWIP : no free socket\n");
                send_reply_open(m, EAGAIN);
                return;
        }

        sock->ops = ops;
        sock->select_ep = NONE;
        sock->recv_data_size = 0;

        if (sock->ops && sock->ops->open)
                ret = sock->ops->open(sock, m);

        if (ret == OK) {
                debug_print("new socket %ld", get_sock_num(sock));
                send_reply_open(m, get_sock_num(sock));
        } else {
                debug_print("failed %d", ret);
                send_reply_open(m, ret);
        }
}

int main(__unused int argc, __unused char ** argv)
{
	sef_local_startup();

	for(;;) {
		int err, ipc_status;
		message m;

		netif_poll_lo();

		mq_process();

		if ((err = sef_receive_status(ANY, &m, &ipc_status)) != OK) {
			printf("LWIP : sef_receive_status errr %d\n", err);
			continue;
		}

		if (m.m_source == VFS_PROC_NR)
			socket_request(&m);
		else if (is_ipc_notify(ipc_status)) {
			switch (m.m_source) {
			case CLOCK:
				expire_timers(m.NOTIFY_TIMESTAMP);
				break;
			case DS_PROC_NR:
				ds_event();
				break;
			case PM_PROC_NR:
				panic("LWIP : unhandled event from PM");
				break;
			default:
				printf("LWIP : unexpected notify from %d\n",
								m.m_source);
				continue;
			}
		} else
			/* all other request can be from drivers only */
			driver_request(&m);
	}

	return 0;
}
