/*
udpstat.c

Created:	March 2001 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#define _POSIX_C_SOURCE 2
#define _NETBSD_SOURCE 1

#include <inet/inet.h>
#undef printf
#undef send

#include <assert.h>
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
#include <net/gen/netdb.h>
#include <net/gen/socket.h>
#include <minix/queryparam.h>

#include <inet/generic/buf.h>
#include <inet/generic/clock.h>
#include <inet/generic/event.h>
#include <inet/generic/type.h>
#include <inet/generic/udp_int.h>

char *prog_name;
udp_fd_t udp_fd_table[UDP_FD_NR];
udp_port_t *udp_port_table;
udp_port_t *udp_port_tablep;
size_t udp_port_table_s;
size_t udp_port_table_rs;
int udp_port_nr;
char values[6 * sizeof(void *) + 3];
char *valuesl= NULL;
size_t v_size;
int inclSel, numerical;

void print_fd(int i, clock_t now);
void usage(void);

int main(int argc, char*argv[])
{
	char *ipstat_device;
	int fd, i, r;
	size_t psize;
	char *pval, *param;
#ifdef __minix_vmd
	struct timeval uptime;
#endif
	clock_t now;
	int fl;
	int a_flag, n_flag;
	struct tms tmsbuf;

	(prog_name=strrchr(argv[0], '/')) ? prog_name++ : (prog_name=argv[0]);

	a_flag= 0;
	n_flag= 0;
	while ((fl= getopt(argc, argv, "?an")) != -1)
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
		default:
			fprintf(stderr, "%s: getopt failed: '%c'\n", 
				prog_name, fl);
			exit(1);
		}
	}
	inclSel= !!a_flag;
	numerical= !!n_flag;

	ipstat_device= IPSTAT_DEVICE;
	if ((fd= open(ipstat_device, O_RDWR)) == -1)
	{
		fprintf(stderr, "%s: unable to open '%s': %s\n", prog_name,
			ipstat_device, strerror(errno));
		exit(1);
	}

	v_size= 2*sizeof(udp_fd_table)+1;
	valuesl= realloc(valuesl, v_size);
	if (!valuesl)
	{
		fprintf(stderr, "%s: unable to malloc %u bytes for valuesl\n",
			prog_name, v_size);
		exit(1);
	}

	param= "udp_fd_table";
	psize= strlen(param);
	r= write(fd, param, psize);
	if (r != psize)
	{
		fprintf(stderr, "%s: write to %s failed: %s\n", prog_name,
			ipstat_device,
			r < 0 ?  strerror(errno) : "short write");
		exit(1);
	}
	r= read(fd, valuesl, v_size);
	if (r < 0)
	{
		fprintf(stderr, "%s: read from %s failed: %s\n", prog_name,
			ipstat_device, strerror(errno));
		exit(1);
	}
	pval= valuesl;
	if (paramvalue(&pval, udp_fd_table, sizeof(udp_fd_table)) !=
		sizeof(udp_fd_table))
	{
		fprintf(stderr,
	"%s: unable to decode the results from queryparam (udp_fd_table)\n",
			prog_name);
		exit(1);
	}

	/* Get address, size, and element size of the UDP port table */
	param = "&udp_port_table,$udp_port_table,$udp_port_table[0]";
	psize = strlen(param);
	r= write(fd, param, psize);
	if (r != psize)
	{
		fprintf(stderr, "%s: write to %s failed: %s\n", prog_name,
			ipstat_device,
			r < 0 ?  strerror(errno) : "short write");
		exit(1);
	}
	r= read(fd, values, sizeof(values));
	if (r < 0)
	{
		fprintf(stderr, "%s: read from %s failed: %s\n", prog_name,
			ipstat_device, strerror(errno));
		exit(1);
	}
	pval= values;
	if (paramvalue(&pval, &udp_port_tablep, sizeof(udp_port_tablep)) !=
		sizeof(udp_port_tablep) ||
		paramvalue(&pval, &udp_port_table_s, sizeof(udp_port_table_s))
			!= sizeof(udp_port_table_s) ||
		paramvalue(&pval, &udp_port_table_rs, sizeof(udp_port_table_rs))
			!= sizeof(udp_port_table_rs))
	{
		fprintf(stderr,
"%s: unable to decode the results from queryparam (&udp_port_table, ...)\n",
			prog_name);
		exit(1);
	}

	if (udp_port_table_rs != sizeof(udp_port_table[0]))
	{
		fprintf(stderr,
	"%s: size mismatch in udp_port_table (different version of inet?)\n",
			prog_name);
		exit(1);
	}
	udp_port_nr= udp_port_table_s/udp_port_table_rs;
	assert(udp_port_table_s == udp_port_nr*udp_port_table_rs);
	udp_port_table= malloc(udp_port_table_s);
	if (!udp_port_table)
	{
		fprintf(stderr,
	"%s: unable to malloc %u bytes for udp_port_table\n",
			prog_name, udp_port_table_s);
		exit(1);
	}
	v_size= 2*udp_port_table_s+1;
	valuesl= realloc(valuesl, v_size);
	if (!valuesl)
	{
		fprintf(stderr, "%s: unable to malloc %u bytes for valuesl\n",
			prog_name, v_size);
		exit(1);
	}

	param = "udp_port_table";
	psize = strlen(param);
	r= write(fd, param, psize);
	if (r != psize)
	{
		fprintf(stderr, "%s: write to %s failed: %s\n", prog_name,
			ipstat_device,
			r < 0 ?  strerror(errno) : "short write");
		exit(1);
	}
	r= read(fd, valuesl, v_size);
	if (r < 0)
	{
		fprintf(stderr, "%s: read from %s failed: %s\n", prog_name,
			ipstat_device, strerror(errno));
		exit(1);
	}
	pval= valuesl;
	if (paramvalue(&pval, udp_port_table, udp_port_table_s) !=
		udp_port_table_s)
	{
		fprintf(stderr,
	"%s: unable to decode the results from queryparam (udp_port_table)\n",
			prog_name);
		exit(1);
	}

	/* Get the uptime in clock ticks. */
#ifdef __minix_vmd
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

	for (i= 0; i<UDP_FD_NR; i++)
		print_fd(i, now);
	exit(0);
}

void print_fd(int i, clock_t now)
{
	unsigned long nwuo_flags;
	int port_nr;
	udp_fd_t *udp_fd;
	udp_port_t *udp_port;
	char *locaddr_str, *remaddr_str;
	struct hostent *hostent;
	struct servent *servent;
	nwio_udpopt_t uf_udpopt;

	udp_fd= &udp_fd_table[i];
	if (!(udp_fd->uf_flags & UFF_INUSE))
		return;
	uf_udpopt= udp_fd->uf_udpopt;
	nwuo_flags= uf_udpopt.nwuo_flags;
	if (((nwuo_flags & NWUO_LOCPORT_MASK) != NWUO_LP_SET) && !inclSel)
		return;

	port_nr= udp_fd->uf_port-udp_port_tablep;
	udp_port= &udp_port_table[port_nr];
	
	printf("%3d", i);

	if (nwuo_flags & NWUO_EN_LOC)
	{
		if (!numerical && (hostent=
			gethostbyaddr((char *)&udp_port->up_ipaddr,
			sizeof(ipaddr_t), AF_INET)) != NULL)
		{
			locaddr_str= hostent->h_name;
		}
		else
			locaddr_str= inet_ntoa(udp_port->up_ipaddr);
	}
	else if (nwuo_flags & NWUO_EN_BROAD)
		locaddr_str= "255.255.255.255";
	else
		locaddr_str= "0.0.0.0";

	printf(" %s:", locaddr_str);

	if ((nwuo_flags & NWUO_LOCPORT_MASK) != NWUO_LP_SEL &&
		(nwuo_flags & NWUO_LOCPORT_MASK) != NWUO_LP_SET)
	{
		printf("*");
	}
	else if ((servent= getservbyport(uf_udpopt.nwuo_locport, "udp")) !=
		NULL)
	{
		printf("%s", servent->s_name);
	}
	else
		printf("%u", ntohs(uf_udpopt.nwuo_locport));

	printf(" -> ");

	if (!(nwuo_flags & NWUO_RA_SET))
		remaddr_str= "*";
	else if (!numerical &&
		(hostent= gethostbyaddr((char *)&uf_udpopt.nwuo_remaddr,
		sizeof(ipaddr_t), AF_INET)) != NULL)
	{
		remaddr_str= hostent->h_name;
	}
	else
		remaddr_str= inet_ntoa(uf_udpopt.nwuo_remaddr);
	printf("%s:", remaddr_str);

	if (!(nwuo_flags & NWUO_RP_SET))
		printf("*");
	else if ((servent= getservbyport(uf_udpopt.nwuo_remport, "udp")) !=
		NULL)
	{
		printf("%s", servent->s_name);
	}
	else
		printf("%u", ntohs(uf_udpopt.nwuo_remport));
	printf("\n");
}

void usage(void)
{
	fprintf(stderr, "Usage: %s [-a] [-n]\n", prog_name);
	exit(1);
}

/*
 * $PchId: udpstat.c,v 1.4 2005/01/30 01:04:57 philip Exp $
 */
