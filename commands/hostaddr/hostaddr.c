/*
hostaddr.c

Fetch an ip and/or ethernet address and print it on one line.

Created:	Jan 27, 1992 by Philip Homburg
*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <net/gen/if_ether.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/ip_io.h>
#include <net/gen/netdb.h>
#include <net/gen/socket.h>
#include <net/gen/nameser.h>
#include <net/gen/resolv.h>
#include <net/gen/dhcp.h>

#include <paths.h>

char *prog_name;

char DHCPCACHE[]=_PATH_DHCPCACHE;

int main( int argc, char *argv[] );
void usage( void );

int main(argc, argv)
int argc;
char *argv[];
{
	int c;
	int first_print;
	int a_flag, e_flag, i_flag, h_flag;
	char *E_arg, *I_arg;
	int do_ether, do_ip, do_asc_ip, do_hostname;
	char *eth_device, *ip_device;
	int eth_fd, ip_fd;
	int result;
	nwio_ethstat_t nwio_ethstat;
	nwio_ipconf_t nwio_ipconf;
	struct hostent *hostent;
	char *hostname, *domain;
	char nodename[2*256];
	dhcp_t dhcp;

	first_print= 1;
	prog_name= argv[0];

	a_flag= e_flag= h_flag = i_flag= 0;
	E_arg= I_arg= NULL;

	while((c= getopt(argc, argv, "?aheE:iI:")) != -1)
	{
		switch(c)
		{
		case '?':
			usage();
		case 'a':
			if (a_flag)
				usage();
			a_flag= 1;
			break;
		case 'h':
			if (h_flag)
				usage();
			h_flag= 1;
			break;
		case 'e':
			if (e_flag)
				usage();
			e_flag= 1;
			break;
		case 'E':
			if (E_arg)
				usage();
			E_arg= optarg;
			break;
		case 'i':
			if (i_flag)
				usage();
			i_flag= 1;
			break;
		case 'I':
			if (I_arg)
				usage();
			I_arg= optarg;
			break;
		default:
			usage();
		}
	}
	if(optind != argc)
		usage();

	do_ether= e_flag;
	if (E_arg)
		eth_device= E_arg;
	else
	{
		eth_device= getenv("ETH_DEVICE");
		if (!eth_device)
			eth_device= ETH_DEVICE;
	}

	do_ip= i_flag;
	do_asc_ip= a_flag;
	do_hostname= h_flag;
	if (I_arg)
		ip_device= I_arg;
	else
	{
		ip_device= getenv("IP_DEVICE");
		if (!ip_device)
			ip_device= IP_DEVICE;
	}

	if (!do_ether && !do_ip && !do_asc_ip && !do_hostname)
		do_ether= do_ip= do_asc_ip= 1;

	if (do_ether)
	{
		eth_fd= open(eth_device, O_RDWR);
		if (eth_fd == -1)
		{
			fprintf(stderr, "%s: Unable to open '%s': %s\n",
				prog_name, eth_device, strerror(errno));
			exit(1);
		}
		result= ioctl(eth_fd, NWIOGETHSTAT, &nwio_ethstat);
		if (result == -1)
		{
			fprintf(stderr, 
			"%s: Unable to fetch ethernet address: %s\n",
				prog_name, strerror(errno));
			exit(1);
		}
		printf("%s%s", first_print ? "" : " ",
					ether_ntoa(&nwio_ethstat.nwes_addr));
		first_print= 0;
	}
	if (do_ip || do_asc_ip || do_hostname)
	{
		ip_fd= open(ip_device, O_RDWR);
		if (ip_fd == -1)
		{
			fprintf(stderr, "%s: Unable to open '%s': %s\n",
				prog_name, ip_device, strerror(errno));
			exit(1);
		}
		result= ioctl(ip_fd, NWIOGIPCONF, &nwio_ipconf);
		if (result == -1)
		{
			fprintf(stderr, 
				"%s: Unable to fetch IP address: %s\n",
				prog_name,
				strerror(errno));
			exit(1);
		}
	}

	setuid(getuid());

	if (do_ip)
	{
		printf("%s%s", first_print ? "" : " ",
					inet_ntoa(nwio_ipconf.nwic_ipaddr));
		first_print= 0;
	}
	if (do_asc_ip || do_hostname)
	{
		int fd;
		int r;
		dhcp_t d;
		u8_t *data;
		size_t hlen, dlen;

		hostname= NULL;
		domain= NULL;

		/* Use a reverse DNS lookup to get the host name.  This is
		 * the preferred method, but often fails due to lazy admins.
		 */
		hostent= gethostbyaddr((char *)&nwio_ipconf.nwic_ipaddr,
			sizeof(nwio_ipconf.nwic_ipaddr), AF_INET);
		if (hostent != NULL) hostname= hostent->h_name;

		if (hostname != NULL)
		{
			/* Reverse DNS works.  */
		}
		else if ((fd= open(DHCPCACHE, O_RDONLY)) == -1)
		{
			if (errno != ENOENT)
			{
				fprintf(stderr, "%s: %s: %s\n",
					prog_name, DHCPCACHE, strerror(errno));
				exit(1);
			}
		}
		else
		{
			/* Try to get the hostname from the DHCP data. */
			while ((r= read(fd, &d, sizeof(d))) == sizeof(d))
			{
				if (d.yiaddr == nwio_ipconf.nwic_ipaddr) break;
			}
			if (r < 0)
			{
				fprintf(stderr, "%s: %s: %s\n",
					prog_name, DHCPCACHE, strerror(errno));
				exit(1);
			}
			close(fd);

			if (r == sizeof(d))
			{
				if (dhcp_gettag(&d, DHCP_TAG_HOSTNAME,
							&data, &hlen))
					hostname= (char *) data;

				if (dhcp_gettag(&d, DHCP_TAG_DOMAIN,
							&data, &dlen))
					domain= (char *) data;

				if (hostname != NULL) hostname[hlen] = 0;
				if (domain != NULL) domain[dlen] = 0;
			}
		}

		if (hostname != NULL)
		{
			if (strchr(hostname, '.') != NULL)
			{
				domain= strchr(hostname, '.');
				*domain++ = 0;
			}
		}
		else
		{
			/* No host name anywhere.  Use the IP address. */
			hostname= inet_ntoa(nwio_ipconf.nwic_ipaddr);
			domain= NULL;
		}

		strcpy(nodename, hostname);
		if (domain != NULL)
		{
			strcat(nodename, ".");
			strcat(nodename, domain);
		}
	}
	if (do_asc_ip)
	{
		printf("%s%s", first_print ? "" : " ", nodename);
		first_print= 0;
	}
	if (do_hostname)
	{
#if __minix_vmd
		if (sysuname(_UTS_SET, _UTS_NODENAME,
					nodename, strlen(nodename)+1) == -1)
		{
			fprintf(stderr, "%s: Unable to set nodename: %s\n",
				prog_name, strerror(errno));
			exit(1);
		}

		if (sysuname(_UTS_SET, _UTS_HOSTNAME,
					hostname, strlen(hostname)+1) == -1)
		{
			fprintf(stderr, "%s: Unable to set hostname: %s\n",
				prog_name, strerror(errno));
			exit(1);
		}
#else
		FILE *fp;

		if ((fp= fopen("/etc/hostname.file", "w")) == NULL
		    || fprintf(fp, "%s\n", nodename) == EOF
		    || fclose(fp) == EOF)
		{
			fprintf(stderr, "%s: /etc/hostname.file: %s\n",
				prog_name, strerror(errno));
			exit(1);
		}
#endif
	}
	if (!first_print) printf("\n");
	exit(0);
}

void usage()
{
	fprintf(stderr,
		"Usage: %s -[eiah] [-E <eth-device>] [-I <ip-device>]\n", 
								prog_name);
	exit(1);
}
