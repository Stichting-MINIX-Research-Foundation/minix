/*
ping.c
*/

#define DEBUG	1

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <net/gen/netdb.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <net/gen/oneCsum.h>
#include <fcntl.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/icmp_hdr.h>
#include <net/gen/ip_io.h>

#define WRITE_SIZE 30
char buffer[16*1024];

int main(int argc, char *argv[]);

#if DEBUG
#define where() fprintf(stderr, "%s %d:", __FILE__, __LINE__);
#endif

#if __STDC__
#define PROTO(x,y) x y
#else
#define PROTO(x,y) X ()
#endif

PROTO (int main, (int argc, char *argv[]) );
static PROTO (void sig_hand, (int signal) );

main(argc, argv)
int argc;
char *argv[];
{
	int fd, i;
	int result, result1;
	nwio_ipopt_t ipopt;
	ip_hdr_t *ip_hdr;
	int ihl;
	icmp_hdr_t *icmp_hdr;
	ipaddr_t dst_addr;
	struct hostent *hostent;
	int length;

	if (argc<2 || argc>3)
	{
		fprintf(stderr, "Usage: %s hostname [-l length] [-t timeout]\n",
			argv[0]);
		exit(1);
	}
	hostent= gethostbyname(argv[1]);
	if (!hostent)
	{
		dst_addr= inet_addr(argv[1]);
		if (dst_addr == -1)
		{
			fprintf(stderr, "%s: unknown host (%s)\n",
				argv[0], argv[1]);
			exit(1);
		}
	}
	else
		dst_addr= *(ipaddr_t *)(hostent->h_addr);
	if (argc == 3)
	{
		length= strtol (argv[2], (char **)0, 0);
		if (length< sizeof(icmp_hdr_t) + IP_MIN_HDR_SIZE)
		{
			fprintf(stderr, "%s: length too small (%s)\n",
				argv[0], argv[2]);
			exit(1);
		}
	}
	else
		length= WRITE_SIZE;

	fd= open ("/dev/ip", O_RDWR);
	if (fd<0)
		perror("open"), exit(1);

	ipopt.nwio_flags= NWIO_COPY | NWIO_PROTOSPEC;
	ipopt.nwio_proto= 1;

	result= ioctl (fd, NWIOSIPOPT, &ipopt);
	if (result<0)
		perror("ioctl (NWIOSIPOPT)"), exit(1);

	result= ioctl (fd, NWIOGIPOPT, &ipopt);
	if (result<0)
		perror("ioctl (NWIOGIPOPT)"), exit(1);

	for (i= 0; i< 20; i++)
	{
		ip_hdr= (ip_hdr_t *)buffer;
		ip_hdr->ih_dst= dst_addr;

		icmp_hdr= (icmp_hdr_t *)(buffer+20);
		icmp_hdr->ih_type= 8;
		icmp_hdr->ih_code= 0;
		icmp_hdr->ih_chksum= 0;
		icmp_hdr->ih_chksum= ~oneC_sum(0, (u16_t *)icmp_hdr,
			WRITE_SIZE-20);
		result= write(fd, buffer, length);
		if (result<0)
		{
			perror("write");
			exit(1);
		}
		if (result != length)
		{
			where();
			fprintf(stderr, "result= %d\n", result);
			exit(1);
		}

		alarm(0);
		signal (SIGALRM, sig_hand);
		alarm(1);

		result= read(fd, buffer, sizeof(buffer));
		if (result>= 0 || errno != EINTR)
			break;
	}
	if (i >= 20)
	{
		printf("no answer from %s\n", argv[1]);
		exit(1);
	}
	if (result<0)
	{
		perror ("read");
		exit(1);
	}
	printf("%s is alive\n", argv[1]);
	exit(0);
}

static void sig_hand(signal)
int signal;
{
}
