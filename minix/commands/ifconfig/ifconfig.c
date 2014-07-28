/*
ifconfig.c
*/

#define _POSIX_C_SOURCE	2

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/ip_io.h>
#include <net/gen/inet.h>

#if __STDC__
#define PROTO(x,y) x y
#else
#define PROTO(x,y) x()
#endif

static PROTO (void usage, (void) );
static PROTO (void set_hostaddr, (int ip_fd, char *host_s, int ins) );
static PROTO (void set_netmask, (int ip_fd, char *net_s, int ins) );
static PROTO (void set_mtu, (int ip_fd, char *mtu_s) );
static PROTO (int check_ipaddrset, (int ip_fd) );
static PROTO (int check_netmaskset, (int ip_fd) );
static PROTO (int get_ipconf, (int ip_fd,
	struct nwio_ipconf *ref_ipconf) );
PROTO (int main, (int argc, char *argv[]) );

char *prog_name;

main(argc, argv)
int argc;
char *argv[];
{
	char *device_s, *hostaddr_s, *mtu_s, *netmask_s, **arg_s;
	int ins;
	int c, ip_fd, ifno;
	struct nwio_ipconf ipconf;
	int i_flag, v_flag, a_flag, modify;
	char *d_arg, *h_arg, *m_arg, *n_arg;

	prog_name= argv[0];

	d_arg= NULL;
	h_arg= NULL;
	m_arg= NULL;
	n_arg= NULL;
	i_flag= 0;
	v_flag= 0;
	a_flag= 0;
	while ((c= getopt(argc, argv, "?I:h:m:n:iva")) != -1)
	{
		switch(c)
		{
		case '?':
			usage();
		case 'I':
			if (d_arg)
				usage();
			d_arg= optarg;
			break;
		case 'h':
			if (h_arg)
				usage();
			h_arg= optarg;
			break;
		case 'm':
			if (m_arg)
				usage();
			m_arg= optarg;
			break;
		case 'n':
			if (n_arg)
				usage();
			n_arg= optarg;
			break;
		case 'i':
			if (i_flag)
				usage();
			i_flag= 1;
			break;
		case 'v':
			if (v_flag)
				usage();
			v_flag= 1;
			break;
		case 'a':
			if (a_flag)
				usage();
			a_flag= 1;
			break;
		default:
			fprintf(stderr, "%s: getopt failed: '%c'\n", 
				prog_name, c);
			exit(1);
		}
	}
	modify = (h_arg != NULL || n_arg != NULL || m_arg != NULL);
	if (a_flag && modify) usage();
	if (!modify) v_flag= 1;

	if (modify) setuid(getuid());

	if (optind != argc)
		usage();

	hostaddr_s= h_arg;
	mtu_s= m_arg;
	netmask_s= n_arg;
	ins= i_flag;

	ifno= 0;
	do
	{
		if (!a_flag) {
			device_s= d_arg;
			if (device_s == NULL)
				device_s= getenv("IP_DEVICE");
			if (device_s == NULL)
				device_s= IP_DEVICE;
		} else {
			static char device[sizeof("/dev/ip99")];

			sprintf(device, "/dev/ip%d", ifno);
			device_s= device;
		}

		ip_fd= open (device_s, O_RDWR);
		if (ip_fd<0)
		{
			if (a_flag && (errno == ENOENT || errno == ENXIO))
				continue;

			fprintf(stderr, "%s: Unable to open '%s': %s\n", 
				prog_name, device_s, strerror(errno));
			exit(1);
		}

		if (hostaddr_s)
			set_hostaddr(ip_fd, hostaddr_s, ins);

		if (netmask_s)
			set_netmask(ip_fd, netmask_s, ins);

		if (mtu_s)
			set_mtu(ip_fd, mtu_s);

		if (v_flag) {
			if (!get_ipconf(ip_fd, &ipconf))
			{
				if (!a_flag)
				{
					fprintf(stderr,
					"%s: %s: Host address not set\n",
						prog_name, device_s);
					exit(1);
				}
			}
			else
			{
				printf("%s: address %s", device_s,
					inet_ntoa(ipconf.nwic_ipaddr));

				if (ipconf.nwic_flags & NWIC_NETMASK_SET)
				{
					printf(" netmask %s",
						inet_ntoa(ipconf.nwic_netmask));
				}
#ifdef NWIC_MTU_SET
				if (ipconf.nwic_mtu)
					printf(" mtu %u", ipconf.nwic_mtu);
#endif
				fputc('\n', stdout);
			}
		}
		close(ip_fd);
	} while (a_flag && ++ifno < 32);
	exit(0);
}

static void set_hostaddr (ip_fd, hostaddr_s, ins)
int ip_fd;
char *hostaddr_s;
int ins;
{
	ipaddr_t ipaddr;
	struct nwio_ipconf ipconf;
	int result;

	ipaddr= inet_addr (hostaddr_s);
	if (ipaddr == (ipaddr_t)(-1))
	{
		fprintf(stderr, "%s: Invalid host address (%s)\n",
			prog_name, hostaddr_s);
		exit(1);
	}
	if (ins && check_ipaddrset(ip_fd))
		return;

	ipconf.nwic_flags= NWIC_IPADDR_SET;
	ipconf.nwic_ipaddr= ipaddr;

	result= ioctl(ip_fd, NWIOSIPCONF, &ipconf);
	if (result<0)
	{
		fprintf(stderr, "%s: Unable to set IP configuration: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}
}

static int check_ipaddrset (ip_fd)
int ip_fd;
{
	struct nwio_ipconf ipconf;

	if (!get_ipconf(ip_fd, &ipconf))
		return 0;

	assert (ipconf.nwic_flags & NWIC_IPADDR_SET);

	return 1;
}

static int get_ipconf (ip_fd, ref_ipaddr)
int ip_fd;
struct nwio_ipconf *ref_ipaddr;
{
	int flags;
	int error, result;
	nwio_ipconf_t ipconf;

	flags= fcntl(ip_fd, F_GETFL);
	fcntl(ip_fd, F_SETFL, flags | O_NONBLOCK);

	result= ioctl (ip_fd, NWIOGIPCONF, &ipconf);
	error= errno;

	fcntl(ip_fd, F_SETFL, flags);

	if (result <0 && error != EAGAIN)
	{
		errno= error;
		fprintf(stderr, "%s: Unable to get IP configuration: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}
	if (result == 0)
	{
		*ref_ipaddr = ipconf;
	}
	return result>=0;
}

static void usage()
{
	fprintf(stderr,
	"Usage: %s [-I ip-device] [-h ipaddr] [-n netmask] [-m mtu] [-iva]\n",
		prog_name);
	exit(1);
}

static void set_netmask (ip_fd, netmask_s, ins)
int ip_fd;
char *netmask_s;
int ins;
{
	ipaddr_t netmask;
	struct nwio_ipconf ipconf;
	int result;

	netmask= inet_addr (netmask_s);
	if (netmask == (ipaddr_t)(-1))
	{
		fprintf(stderr, "%s: Invalid netmask (%s)\n",
			prog_name, netmask_s);
		exit(1);
	}
	if (ins && check_netmaskset(ip_fd))
		return;

	ipconf.nwic_flags= NWIC_NETMASK_SET;
	ipconf.nwic_netmask= netmask;

	result= ioctl(ip_fd, NWIOSIPCONF, &ipconf);
	if (result<0)
	{
		fprintf(stderr, "%s: Unable to set IP configuration: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}
}

static void set_mtu (ip_fd, mtu_s)
int ip_fd;
char *mtu_s;
{
	ipaddr_t netmask;
	int result;
	long mtu;
	char *check;
	struct nwio_ipconf ipconf;

	mtu= strtol (mtu_s, &check, 0);
	if (check[0] != '\0')
	{
		fprintf(stderr, "%s: Invalid mtu (%s)\n",
			prog_name, mtu_s);
		exit(1);
	}

#ifdef NWIC_MTU_SET
	ipconf.nwic_flags= NWIC_MTU_SET;
	ipconf.nwic_mtu= mtu;

	result= ioctl(ip_fd, NWIOSIPCONF, &ipconf);
	if (result<0)
	{
		fprintf(stderr, "%s: Unable to set IP configuration: %s\n",
			prog_name, strerror(errno));
		exit(1);
	}
#endif
}

static int check_netmaskset (ip_fd)
int ip_fd;
{
	struct nwio_ipconf ipconf;

	if (!get_ipconf(ip_fd, &ipconf))
	{
		fprintf(stderr,
"%s: Unable to determine if netmask is set, please set IP address first\n",
			prog_name);
		exit(1);
	}

	return (ipconf.nwic_flags & NWIC_NETMASK_SET);
}

/*
 * $PchId: ifconfig.c,v 1.7 2001/02/21 09:19:52 philip Exp $
 */
