/*
socket.c

Created:	Feb 2001 by Philip Homburg <philip@f-mnx.phicoh.com>

Open a TCP connection
*/

#define _POSIX_C_SOURCE 2

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/wait.h>

#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/netdb.h>
#include <net/gen/socket.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#define BUF_SIZE	10240

char *progname;
int tcpfd= -1;
char buf[BUF_SIZE];
static int bulk= 0;
static int push= 0;
static int stdout_issocket= 0;
static int timeout;

static void do_conn(char *hostname, char *portname);
static void alrm_conn(int sig);
static void alrm_io(int sig);
static void fullduplex(void);
static void fatal(char *msg, ...);
static void usage(void);

int main(int argc, char *argv[])
{
	int c;
	char *hostname;
	char *portname;
	char *check;
	int B_flag, P_flag, s_flag;
	char *t_arg;

	(progname=strrchr(argv[0],'/')) ? progname++ : (progname=argv[0]);

	B_flag= 0;
	P_flag= 0;
	s_flag= 0;
	t_arg= NULL;
	while (c= getopt(argc, argv, "BPst:?"), c != -1)
	{
		switch(c)
		{
		case 'B':	B_flag= 1; break;
		case 'P':	P_flag= 1; break;
		case 's':	s_flag= 1; break;
		case 't':	t_arg= optarg; break;
		case '?':	usage();
		default:
			fatal("getopt failed: '%c'", c);
		}
	}
	if (t_arg)
	{
		timeout= strtol(t_arg, &check, 0);
		if (check[0] != '\0')
			fatal("unable to parse timeout '%s'\n", t_arg);
		if (timeout <= 0)
			fatal("bad timeout '%d'\n", timeout);
	}
	else
		timeout= 0;

	if (optind+2 != argc)
		usage();
	hostname= argv[optind++];
	portname= argv[optind++];

	bulk= B_flag;
	push= P_flag;
	stdout_issocket= s_flag;

	do_conn(hostname, portname);

	/* XXX */
	if (timeout)
	{
		signal(SIGALRM, alrm_io);
		alarm(timeout);
	}

	fullduplex();
	exit(0);
}

static void do_conn(char *hostname, char *portname)
{
	ipaddr_t addr;
	tcpport_t port;
	struct hostent *he;
	struct servent *se;
	char *tcp_device, *check;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpcl;
	nwio_tcpopt_t tcpopt;

	if (!inet_aton(hostname, &addr))
	{
		he= gethostbyname(hostname);
		if (he == NULL)
			fatal("unknown hostname '%s'", hostname);
		if (he->h_addrtype != AF_INET || he->h_length != sizeof(addr))
			fatal("bad address for '%s'", hostname);
		memcpy(&addr, he->h_addr, sizeof(addr));
	}

	port= strtol(portname, &check, 0);
	if (check[0] != 0)
	{
		se= getservbyname(portname, "tcp");
		if (se == NULL)
			fatal("unkown port '%s'", portname);
		port= ntohs(se->s_port);
	}

	tcp_device= getenv("TCP_DEVICE");
	if (tcp_device == NULL) tcp_device= TCP_DEVICE;

	tcpfd= open(tcp_device, O_RDWR);
	if (tcpfd == -1)
		fatal("unable to open '%s': %s", tcp_device, strerror(errno));
	tcpconf.nwtc_flags= NWTC_EXCL | NWTC_LP_SEL | NWTC_SET_RA |
		NWTC_SET_RP;
	tcpconf.nwtc_remaddr= addr;
	tcpconf.nwtc_remport= htons(port);;
	if (ioctl(tcpfd, NWIOSTCPCONF, &tcpconf) == -1)
		fatal("NWIOSTCPCONF failed: %s", strerror(errno));

	if (timeout)
	{
		signal(SIGALRM, alrm_conn);
		alarm(timeout);
	}

	tcpcl.nwtcl_flags= 0;
	if (ioctl(tcpfd, NWIOTCPCONN, &tcpcl) == -1)
	{
		fatal("unable to connect to %s:%u: %s", inet_ntoa(addr),
			ntohs(tcpconf.nwtc_remport), strerror(errno));
	}

	alarm(0);

	if (bulk)
	{
		tcpopt.nwto_flags= NWTO_BULK;
		if (ioctl(tcpfd, NWIOSTCPOPT, &tcpopt) == -1)
			fatal("NWIOSTCPOPT failed: %s", strerror(errno));
	}
}

static void alrm_conn(int sig)
{
	fatal("timeout during connect");
}

static void alrm_io(int sig)
{
	fatal("timeout during io");
}

static void fullduplex(void)
{
	pid_t cpid;
	int o, r, s, s_errno, loc;

	cpid= fork();
	switch(cpid)
	{
	case -1:	fatal("fork failed: %s", strerror(errno));
	case 0:
		/* Read from TCP, write to stdout. */
		for (;;)
		{
			r= read(tcpfd, buf, BUF_SIZE);
			if (r == 0)
				break;
			if (r == -1)
			{
				r= errno;
				if (stdout_issocket)
					ioctl(1, NWIOTCPSHUTDOWN, NULL);
				fatal("error reading from TCP conn.: %s",
					strerror(errno));
			}
			s= r; 
			for (o= 0; o<s; o += r)
			{
				r= write(1, buf+o, s-o);
				if (r <= 0)
				{
					fatal("error writing to stdout: %s",
						r == 0 ? "EOF" :
						strerror(errno));
				}
			}
		}
		if (stdout_issocket)
		{
			r= ioctl(1, NWIOTCPSHUTDOWN, NULL);
			if (r == -1)
			{
				fatal("NWIOTCPSHUTDOWN failed on stdout: %s",
					strerror(errno));
			}
		}
		exit(0);
	default:
		break;
	}

	/* Read from stdin, write to TCP. */
	for (;;)
	{
		r= read(0, buf, BUF_SIZE);
		if (r == 0)
			break;
		if (r == -1)
		{
			s_errno= errno;
			kill(cpid, SIGTERM);
			fatal("error reading from stdin: %s",
				strerror(s_errno));
		}
		s= r; 
		for (o= 0; o<s; o += r)
		{
			r= write(tcpfd, buf+o, s-o);
			if (r <= 0)
			{
				s_errno= errno;
				kill(cpid, SIGTERM);
				fatal("error writing to TCP conn.: %s",
					r == 0 ? "EOF" :
					strerror(s_errno));
			}
		}
		if (push)
			ioctl(tcpfd, NWIOTCPPUSH, NULL);
	}
	if (ioctl(tcpfd, NWIOTCPSHUTDOWN, NULL) == -1)
	{
		s_errno= errno;
		kill(cpid, SIGTERM);
		fatal("unable to shut down TCP conn.: %s", strerror(s_errno));
	}

	r= waitpid(cpid, &loc, 0);
	if (r == -1)
	{
		s_errno= errno;
		kill(cpid, SIGTERM);
		fatal("waitpid failed: %s", strerror(s_errno));
	}
	if (WIFEXITED(loc))
		exit(WEXITSTATUS(loc));
	kill(getpid(), WTERMSIG(loc));
	exit(1);
}

static void fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-BPs] [-t timeout] hostname portname\n",
		progname);
	exit(1);
}

/*
 * $PchId: socket.c,v 1.3 2005/01/31 22:33:20 philip Exp $
 */
