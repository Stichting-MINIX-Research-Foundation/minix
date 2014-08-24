/*
tcpstat.c

Created:	June 1995 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#define _POSIX_C_SOURCE 2
#define _NETBSD_SOURCE 1

#include <inet/inet.h>
#undef printf
#undef send

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/svrctl.h>
#ifndef __minix_vmd
#include <sys/times.h>
#endif
#include <net/netlib.h>
#include <net/gen/inet.h>
#include <netdb.h>
#include <net/gen/socket.h>
#include <minix/queryparam.h>
#include <minix/com.h>

#include <inet/generic/buf.h>
#include <inet/generic/clock.h>
#include <inet/generic/event.h>
#include <inet/generic/type.h>
#include <inet/generic/tcp.h>
#include <inet/generic/tcp_int.h>

u32_t system_hz;
char *prog_name;
tcp_conn_t tcp_conn_table[TCP_CONN_NR];
char values[2 * sizeof(tcp_conn_table) + 1];
int inclListen, numerical, verbose;

void print_conn(int i, clock_t now);
void usage(void);

int main(int argc, char*argv[])
{
	char *ipstat_device;
	int fd, i, r;
	char *query, *pval;
	size_t len;
#ifdef __minix_vmd
	struct timeval uptime;
#endif
	clock_t now;
	int fl;
	int a_flag, n_flag, v_flag;
	struct tms tmsbuf;

	system_hz = (u32_t) sysconf(_SC_CLK_TCK);

	(prog_name=strrchr(argv[0], '/')) ? prog_name++ : (prog_name=argv[0]);

	a_flag= 0;
	n_flag= 0;
	v_flag= 0;
	while ((fl= getopt(argc, argv, "?anv")) != -1)
	{
		switch(fl)
		{
		case '?':
			usage();
		case 'a':
			a_flag= 1;
			break;
		case 'n':
			n_flag= 1;
			break;
		case 'v':
			v_flag= 1;
			break;
		default:
			fprintf(stderr, "%s: getopt failed: '%c'\n", 
				prog_name, fl);
			exit(1);
		}
	}
	inclListen= !!a_flag;
	numerical= !!n_flag;
	verbose= !!v_flag;

	ipstat_device= IPSTAT_DEVICE;
	if ((fd= open(ipstat_device, O_RDWR)) == -1)
	{
		fprintf(stderr, "%s: unable to open '%s': %s\n", prog_name,
			ipstat_device, strerror(errno));
		exit(1);
	}

	query= "tcp_conn_table";
	len= strlen(query);
	r= write(fd, query, len);
	if (r != len)
	{
		fprintf(stderr, "%s: write to %s failed: %s\n",
			prog_name, ipstat_device, r < 0 ? strerror(errno) :
			"short write");
		exit(1);
	}
	r= read(fd, values, sizeof(values));
	if (r == -1)
	{
		fprintf(stderr, "%s: read from %s failed: %s\n", prog_name,
			ipstat_device, strerror(errno));
		exit(1);
	}
	pval= values;
	if (paramvalue(&pval, tcp_conn_table, sizeof(tcp_conn_table)) !=
		sizeof(tcp_conn_table))
	{
		fprintf(stderr,
			"%s: unable to decode the results from queryparam\n",
			prog_name);
		exit(1);
	}

#ifdef __minix_vmd
	/* Get the uptime in clock ticks. */
	if (sysutime(UTIME_UPTIME, &uptime) == -1)
	{
		fprintf(stderr, "%s: sysutime failed: %s\n", prog_name,
			strerror(errno));
		exit(1);
	}
	now= uptime.tv_sec * HZ + (uptime.tv_usec*HZ/1000000);
#else	/* Minix 3 */
	now= times(&tmsbuf);
#endif

	for (i= 0; i<TCP_CONN_NR; i++)
		print_conn(i, now);
	exit(0);
}

void print_conn(int i, clock_t now)
{
	tcp_conn_t *tcp_conn;
	char *addr_str;
	struct hostent *hostent;
	struct servent *servent;
	ipaddr_t a1, a2;
	tcpport_t p1, p2;
	unsigned flags;
	int no_verbose;
	clock_t rtt, artt, drtt;

	tcp_conn= &tcp_conn_table[i];
	if (!(tcp_conn->tc_flags & TCF_INUSE))
		return;
	if (tcp_conn->tc_state == TCS_LISTEN && !inclListen)
		return;
	if (tcp_conn->tc_state == TCS_CLOSED && tcp_conn->tc_fd == NULL &&
		tcp_conn->tc_senddis < now)
	{
		return;
	}
	
	printf("%3d", i);

	a1= tcp_conn->tc_locaddr;
	p1= tcp_conn->tc_locport;
	a2= tcp_conn->tc_remaddr;
	p2= tcp_conn->tc_remport;

	if (a1 == 0)
		addr_str= "*";
	else if (!numerical &&
		(hostent= gethostbyaddr((char *)&a1,
		sizeof(a1), AF_INET)) != NULL)
	{
		addr_str= hostent->h_name;
	}
	else
		addr_str= inet_ntoa(a1);
	printf(" %s:", addr_str);

	if (p1 == 0)
		printf("*");
	else if ((servent= getservbyport(p1, "tcp")) != NULL)
	{
		printf("%s", servent->s_name);
	}
	else
		printf("%u", ntohs(p1));

	if (tcp_conn->tc_orglisten)
		printf(" <- ");
	else
		printf(" -> ");

	if (a2 == 0)
		addr_str= "*";
	else if (!numerical &&
		(hostent= gethostbyaddr((char *)&a2,
		sizeof(a2), AF_INET)) != NULL)
	{
		addr_str= hostent->h_name;
	}
	else
		addr_str= inet_ntoa(a2);
	printf("%s:", addr_str);

	if (p2 == 0)
		printf("*");
	else if ((servent= getservbyport(p2, "tcp")) !=
		NULL)
	{
		printf("%s", servent->s_name);
	}
	else
		printf("%u", ntohs(p2));

	printf(" ");
	no_verbose= 0;
	switch(tcp_conn->tc_state)
	{
	case TCS_CLOSED:	printf("CLOSED");
				if (tcp_conn->tc_senddis >= now)
				{
					printf("(time wait %d s)",
					(tcp_conn->tc_senddis-now)/system_hz);
				}
				no_verbose= 1;
				break;
	case TCS_LISTEN:	printf("LISTEN"); no_verbose= 1; break;
	case TCS_SYN_RECEIVED:	printf("SYN_RECEIVED"); break;
	case TCS_SYN_SENT:	printf("SYN_SENT"); break;
	case TCS_ESTABLISHED:	printf("ESTABLISHED"); break;
	case TCS_CLOSING:	printf("CLOSING"); break;
	default:		printf("state(%d)", tcp_conn->tc_state);
				break;
	}

	if (tcp_conn->tc_flags & TCF_FIN_RECV)
		printf(" F<");
	if (tcp_conn->tc_flags & TCF_FIN_SENT)
	{
		printf(" F>");
		if (tcp_conn->tc_SND_UNA == tcp_conn->tc_SND_NXT)
			printf("+");
	}
	if (tcp_conn->tc_state != TCS_CLOSED &&
		tcp_conn->tc_state != TCS_LISTEN)
	{
		printf("\n\t");
		printf("RQ: %u, SQ: %u, RWnd: %u, SWnd: %u, SWThresh: %u",
			tcp_conn->tc_RCV_NXT - tcp_conn->tc_RCV_LO,
			tcp_conn->tc_SND_NXT - tcp_conn->tc_SND_UNA,
			tcp_conn->tc_rcv_wnd,
			tcp_conn->tc_snd_cwnd - tcp_conn->tc_SND_UNA,
			tcp_conn->tc_snd_cthresh);
	}

	printf("\n");

	if (!verbose || no_verbose)
		return;
	rtt= tcp_conn->tc_rtt;
	artt= tcp_conn->tc_artt;
	drtt= tcp_conn->tc_drtt;
	printf("\tmss %u, mtu %u%s, rtt %.3f (%.3f+%d*%.3f) s\n",
		tcp_conn->tc_max_mtu-IP_TCP_MIN_HDR_SIZE,
		tcp_conn->tc_mtu,
		(tcp_conn->tc_flags & TCF_PMTU) ? "" : " (no PMTU)",
		rtt/(system_hz+0.0),
		artt/(system_hz+0.0)/TCP_RTT_SCALE, TCP_DRTT_MULT,
		drtt/(system_hz+0.0)/TCP_RTT_SCALE);
	flags= tcp_conn->tc_flags;
	printf("\tflags:");
	if (!flags)
		printf(" TCF_EMPTY");
	if (flags & TCF_INUSE)
		flags &= ~TCF_INUSE;
	if (flags & TCF_FIN_RECV)
	{
		printf(" TCF_FIN_RECV");
		flags &= ~TCF_FIN_RECV;
	}
	if (flags & TCF_RCV_PUSH)
	{
		printf(" TCF_RCV_PUSH");
		flags &= ~TCF_RCV_PUSH;
	}
	if (flags & TCF_MORE2WRITE)
	{
		printf(" TCF_MORE2WRITE");
		flags &= ~TCF_MORE2WRITE;
	}
	if (flags & TCF_SEND_ACK)
	{
		printf(" TCF_SEND_ACK");
		flags &= ~TCF_SEND_ACK;
	}
	if (flags & TCF_FIN_SENT)
	{
		printf(" TCF_FIN_SENT");
		flags &= ~TCF_FIN_SENT;
	}
	if (flags & TCF_BSD_URG)
	{
		printf(" TCF_BSD_URG");
		flags &= ~TCF_BSD_URG;
	}
	if (flags & TCF_NO_PUSH)
	{
		printf(" TCF_NO_PUSH");
		flags &= ~TCF_NO_PUSH;
	}
	if (flags & TCF_PUSH_NOW)
	{
		printf(" TCF_PUSH_NOW");
		flags &= ~TCF_PUSH_NOW;
	}
	if (flags & TCF_PMTU)
		flags &= ~TCF_PMTU;
	if (flags)
		printf(" 0x%x", flags);
	printf("\n");
	printf("\ttimer: ref %d, time %f, active %d\n",
		tcp_conn->tc_transmit_timer.tim_ref,
		(0.0+tcp_conn->tc_transmit_timer.tim_time-now)/system_hz,
		tcp_conn->tc_transmit_timer.tim_active);
}

void usage(void)
{
	fprintf(stderr, "Usage: %s [-anv]\n", prog_name);
	exit(1);
}

/*
 * $PchId: tcpstat.c,v 1.8 2005/01/30 01:04:38 philip Exp $
 */
