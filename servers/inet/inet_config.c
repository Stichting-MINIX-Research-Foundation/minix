/*
inet/inet_config.c

Created:	Nov 11, 1992 by Philip Homburg

Modified:	Apr 07, 2001 by Kees J. Bot
		Read the configuration file and fill in the xx_conf[] arrays.

Copyright 1995 Philip Homburg
*/

#define _MINIX_SOURCE 1
#define _POSIX_SOURCE 1

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <minix/type.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include "inet_config.h"
#include "inet.h"

THIS_FILE

struct eth_conf eth_conf[IP_PORT_MAX];
struct psip_conf psip_conf[IP_PORT_MAX];
struct ip_conf ip_conf[IP_PORT_MAX];
struct tcp_conf tcp_conf[IP_PORT_MAX];
struct udp_conf udp_conf[IP_PORT_MAX];
dev_t ip_dev;

int eth_conf_nr;
int psip_conf_nr;
int ip_conf_nr;
int tcp_conf_nr;
int udp_conf_nr;

int ip_forward_directed_bcast= 0;	/* Default is off */

static u8_t iftype[IP_PORT_MAX];	/* Interface in use as? */
static int ifdefault= -1;		/* Default network interface. */

static void fatal(char *label)
{
	printf("init: %s: %s\n", label, strerror(errno));
	exit(1);
}

static void check_rm(char *device)
/* Check if a device is not among the living. */
{
	if (unlink(device) < 0) {
		if (errno == ENOENT) return;
		fatal(device);
	}
	printf("rm %s\n", device);
}

static void check_mknod(char *device, mode_t mode, int minor)
/* Check if a device exists with the proper device number. */
{
	struct stat st;
	dev_t dev;

	dev= makedev(major(ip_dev), minor);

	if (stat(device, &st) < 0) {
		if (errno != ENOENT) fatal(device);
	} else {
		if (S_ISCHR(st.st_mode) && st.st_rdev == dev) return;
		if (unlink(device) < 0) fatal(device);
	}

	if (mknod(device, S_IFCHR | mode, dev) < 0) fatal(device);
	printf("mknod %s c %d %d\n", device, major(ip_dev), minor);
}

static void check_ln(char *old, char *new)
/* Check if 'old' and 'new' are still properly linked. */
{
	struct stat st_old, st_new;

	if (stat(old, &st_old) < 0) fatal(old);
	if (stat(new, &st_new) < 0) {
		if (errno != ENOENT) fatal(new);
	} else {
		if (st_new.st_dev == st_old.st_dev
					&& st_new.st_ino == st_old.st_ino) {
			return;
		}
		if (unlink(new) < 0) fatal(new);
	}

	if (link(old, new) < 0) fatal(new);
	printf("ln %s %s\n", old, new);
}

static void check_dev(int type, int ifno)
/* Check if the device group with interface number 'ifno' exists and has the
 * proper device numbers.  If 'type' is -1 then the device group must be
 * removed.
 */
{
	static struct devlist {
		char	*defname;
		mode_t	mode;
		u8_t	minor_off;
	} devlist[] = {
		{	"/dev/eth",	0600,	ETH_DEV_OFF	},
		{	"/dev/psip",	0600,	PSIP_DEV_OFF	},
		{	"/dev/ip",	0600,	IP_DEV_OFF	},
		{	"/dev/tcp",	0666,	TCP_DEV_OFF	},
		{	"/dev/udp",	0666,	UDP_DEV_OFF	},
	};
	struct devlist *dvp;
	int i;
	char device[sizeof("/dev/psip99")];
	char *dp;

	for (i= 0; i < sizeof(devlist) / sizeof(devlist[0]); i++) {
		dvp= &devlist[i];
		strcpy(device, dvp->defname);
		dp= device + strlen(device);
		if (ifno >= 10) *dp++ = '0' + (ifno / 10);
		*dp++ = '0' + (ifno % 10);
		*dp = 0;

		if (type == 0
			|| (i == 0 && type != NETTYPE_ETH)
			|| (i == 1 && type != NETTYPE_PSIP)
		) {
			check_rm(device);
			if (ifno == ifdefault) check_rm(dvp->defname);
		} else {
			check_mknod(device, dvp->mode,
				if2minor(ifno, dvp->minor_off));
			if (ifno == ifdefault) check_ln(device, dvp->defname);
		}
	}
	check_mknod(IPSTAT_DEV, IPSTAT_MODE, IPSTAT_MINOR);
}

static int cfg_fd;
static char word[16];
static unsigned char line[256], *lineptr;
static unsigned linenr;

static void error(void)
{
	printf("inet: error on line %u\n", linenr);
	exit(1);
}

static int nextline(void)
{
	/* Read a line from the configuration file, to be used by subsequent
	 * token() calls. Skip empty lines, and lines where the first character
	 * after leading "whitespace" is '#'. The last line of the file need
	 * not be terminated by a newline. Return 1 if a line was read in
	 * successfully, and 0 on EOF or error.
	 */
	unsigned char *lp, c;
	int r, skip;

	lineptr = lp = line;
	linenr++;
	skip = -1;

	while ((r = read(cfg_fd, &c, 1)) == 1) {
		if (c == '\n') {
			if (skip == 0)
				break;

			linenr++;
			skip = -1;
			continue;
		}

		if (skip == -1 && c > ' ')
			skip = (c == '#');

		if (skip == 0 && lp < (unsigned char *) line + sizeof(line)-1)
			*lp++ = c;
	}

	*lp = 0;
	return (r == 1 || lp != line);
}

static void token(int need)
{
	/* Read a word from the configuration line.  Return a null string on
	 * EOL.  Return a punctuation as a one character word.  If 'need' is
	 * true then an actual word is expected at this point, so err out if
	 * not.
	 */
	unsigned char *wp;
	static unsigned char c= '\n';

	wp= (unsigned char *) word;
	*wp = 0;

	while (c <= ' ') {
		if (*lineptr == 0) {
			if (need) error();
			return;
		}
		c = *lineptr++;
	}

	do {
		if (wp < (unsigned char *) word + sizeof(word)-1) *wp++ = c;
		c = (*lineptr != 0) ? *lineptr++ : ' ';
		if (word[0] == ';' || word[0] == '{' || word[0] == '}') {
			if (need) error();
			break;
		}
	} while (c > ' ' && c != ';' && c != '{' && c != '}');
	*wp = 0;
}

static unsigned number(char *str, unsigned max)
{
	/* Interpret a string as an unsigned decimal number, no bigger than
	 * 'max'.  Return this number.
	 */
	char *s;
	unsigned n, d;

	s= str;
	n= 0;
	while ((d= (*s - '0')) < 10 && n <= max) {
		n= n * 10 + d;
		s++;
	}
	if (*s != 0 || n > max) {
		printf("inet: '%s' is not a number <= %u\n", str, max);
		error();
	}
	return n;
}

void read_conf(void)
{
	int i, j, ifno = -1, type = -1, port = -1, enable;
	struct eth_conf *ecp;
	struct psip_conf *pcp;
	struct ip_conf *icp;
	struct stat st;
	char buf[sizeof(word)];

	{ static int first= 1; 
		if (!first) ip_panic(( "read_conf: called a second time" ));
		first= 0;
#if 0
		*(u8_t *)0 = 0xcc;	/* INT 3 */
#endif
	}


	/* Open the configuration file. */
	if ((cfg_fd= open(PATH_INET_CONF, O_RDONLY)) == -1)
		fatal(PATH_INET_CONF);

	ecp= eth_conf;
	pcp= psip_conf;
	icp= ip_conf;

	while (nextline()) {
		token(1);
		if (strncmp(word, "eth", 3) == 0) {
			ecp->ec_ifno= ifno= number(word+3, IP_PORT_MAX-1);
			type= NETTYPE_ETH;
			port= eth_conf_nr;
			token(1);
			if (strcmp(word, "vlan") == 0) {
				token(1);
				ecp->ec_vlan= number(word, (1<<12)-1);
				token(1);
				if (strncmp(word, "eth", 3) != 0) {
					printf(
				"inet: VLAN eth%d can't be built on %s\n",
						ifno, word);
					exit(1);
				}
				ecp->ec_port= number(word+3, IP_PORT_MAX-1);
			} else {
				/* The process label consists of the driver
				 * name, an underscore, and the instance
				 * number.
				 */
				strncpy(buf, word, sizeof(buf)-1);
				buf[sizeof(buf)-1]= 0;
				token(1);
				ecp->ec_label=
					alloc(strlen(buf)+1+strlen(word)+1);
				sprintf(ecp->ec_label, "%s_%s", buf, word);
				ecp->ec_port= 0;
			}
			ecp++;
			eth_conf_nr++;
		} else if (strncmp(word, "psip", 4) == 0) {
			pcp->pc_ifno= ifno= number(word+4, IP_PORT_MAX-1);
			type= NETTYPE_PSIP;
			port= psip_conf_nr;
			pcp++;
			psip_conf_nr++;
		} else {
			printf("inet: Unknown device '%s'\n", word);
			error();
		}

		if (type == -1 || ifno == -1 || port == -1) {
			printf("inet: faulty configuration\n");
			exit(1);
		}

		iftype[ifno]= type;
		icp->ic_ifno= ifno;
		icp->ic_devtype= type;
		icp->ic_port= port;
		tcp_conf[tcp_conf_nr].tc_port= ip_conf_nr;
		udp_conf[udp_conf_nr].uc_port= ip_conf_nr;

		enable= 7;	/* 1 = IP, 2 = TCP, 4 = UDP */

		token(0);
		if (word[0] == '{') {
			token(0);
			while (word[0] != '}') {
				if (strcmp(word, "default") == 0) {
					if (ifdefault != -1) {
						printf(
				"inet: ip%d and ip%d can't both be default\n",
							ifdefault, ifno);
						error();
					}
					ifdefault= ifno;
					token(0);
				} else
				if (strcmp(word, "no") == 0) {
					token(1);
					if (strcmp(word, "ip") == 0) {
						enable= 0;
					} else
					if (strcmp(word, "tcp") == 0) {
						enable &= ~2;
					} else
					if (strcmp(word, "udp") == 0) {
						enable &= ~4;
					} else {
						printf(
						"inet: Can't do 'no %s'\n",
							word);
						exit(1);
					}
					token(0);
				} else {
					printf("inet: Unknown option '%s'\n",
						word);
					exit(1);
				}
				if (word[0] == ';') token(0);
				else
				if (word[0] != '}') error();
			}
			token(0);
		}
		if (word[0] != ';' && word[0] != 0) error();

		if (enable & 1) icp++, ip_conf_nr++;
		if (enable & 2) tcp_conf_nr++;
		if (enable & 4) udp_conf_nr++;
	}

	if (ifdefault == -1) {
		printf("inet: No networks or no default network defined\n");
		exit(1);
	}

	/* Translate VLAN network references to port numbers. */
	for (i= 0; i < eth_conf_nr; i++) {
		ecp= &eth_conf[i];
		if (eth_is_vlan(ecp)) {
			for (j= 0; j < eth_conf_nr; j++) {
				if (eth_conf[j].ec_ifno == ecp->ec_port
					&& !eth_is_vlan(&eth_conf[j])
				) {
					ecp->ec_port= j;
					break;
				}
			}
			if (j == eth_conf_nr) {
				printf(
				"inet: VLAN eth%d can't be built on eth%d\n",
					ecp->ec_ifno, ecp->ec_port);
				exit(1);
			}
		}
	}

	/* Set umask 0 so we can creat mode 666 devices. */
	(void) umask(0);

	/* See what the device number of /dev/ip is.  That's what we
	 * used last time for the network devices, so we keep doing so.
	 */
	if (stat("/dev/ip", &st) < 0) fatal("/dev/ip");
	ip_dev= st.st_rdev;

	for (i= 0; i < IP_PORT_MAX; i++) {
		/* Create network devices. */
		check_dev(iftype[i], i);
	}
}

void *alloc(size_t size)
{
	/* Allocate memory on the heap with sbrk(). */

	return malloc(size);
}

/*
 * $PchId: inet_config.c,v 1.10 2003/08/21 09:26:02 philip Exp $
 */
