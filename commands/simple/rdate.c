/*	rdate 1.0 - Set time&date from remote host	Author: Kees J. Bot
 *								12 Oct 1995
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/netdb.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

void report(const char *label)
{
	fprintf(stderr, "rdate: %s: %s\n", label, strerror(errno));
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

int main(int argc, char **argv)
{
	char *tcp_device;
	int fd;
	int i;
	struct servent *servent;
	struct hostent *hostent;
	u16_t time_port;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpcl;
	u32_t net_time;
	time_t unix_time;

	if (argc <= 1) {
		fprintf(stderr, "Usage: rdate host ...\n");
		exit(1);
	}

	/* Look up the port number of the TCP service "time". */
	if ((servent= getservbyname("time", "tcp")) == nil) {
		fprintf(stderr, "rdate: \"time\": unknown service\n");
		exit(1);
	}
	time_port= servent->s_port;

	if ((tcp_device= getenv("TCP_DEVICE")) == nil) tcp_device= TCP_DEVICE;

	if ((fd= open(tcp_device, O_RDWR)) < 0) fatal(tcp_device);

	/* Try each host on the command line. */
	for (i= 1; i < argc; i++) {
		if ((hostent= gethostbyname(argv[i])) == nil) {
			fprintf(stderr, "rdate: %s: unknown host\n", argv[i]);
			continue;
		}

		/* Configure a TCP channel and connect to the remote host. */

		tcpconf.nwtc_flags= NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
		memcpy(&tcpconf.nwtc_remaddr, hostent->h_addr, 4);
		tcpconf.nwtc_remport= time_port;
		if (ioctl(fd, NWIOSTCPCONF, &tcpconf) == -1) fatal(tcp_device);

		tcpcl.nwtcl_flags= 0;
		if (ioctl(fd, NWIOTCPCONN, &tcpcl) < 0) {
			report(argv[i]);
			continue;
		}

		/* Read four bytes to obtain the time. */
		switch (read(fd, &net_time, sizeof(net_time))) {
		case -1:
			report(argv[i]);
			continue;
		default:
			fprintf(stderr, "rdate: %s: short read\n", argv[i]);
			continue;
		case sizeof(net_time):
			break;
		}
		break;
	}
	if (i == argc) exit(1);

	/* Internet time is in seconds since 1900, UNIX time is in seconds
	 * since 1970.
	 */
	unix_time= ntohl(net_time) - 2208988800;

	/* Try to set the time and tell us about it. */
	if (stime(&unix_time) < 0) {
		printf("time on ");
	} else {
		printf("time set to ");
	}
	printf("%s: %s", argv[i], ctime(&unix_time));
	exit(0);
}
