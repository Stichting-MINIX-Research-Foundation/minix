/*
inet/inet_config.c

Created:	Nov 11, 1992 by Philip Homburg

Modified:	Apr 07, 2001 by Kees J. Bot
		Read the configuration file and fill in the xx_conf[] arrays.

Copyright 1995 Philip Homburg
*/

#define _POSIX_SOURCE 1
#define _NETBSD_SOURCE 1

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

#include "proto.h"
#include <minix/netsock.h>


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

static int ifdefault= -1;		/* Default network interface. */

static void fatal(char *label)
{
	printf("init: %s: %s\n", label, strerror(errno));
	exit(1);
}

static void check_mknod(char *device, mode_t mode, int minor)
/* Check if a device exists with the proper device number. */
{
	dev_t dev;

	dev= (ip_dev & 0xFF00) | minor;

	unlink(device);
	if (mknod(device, S_IFCHR | mode, dev) < 0) fatal(device);
	printf("mknod %s c %d %d\n", device, (ip_dev >> 8), minor);
}

static int cfg_fd;
static char word[16];
static unsigned char line[256], *lineptr;
static unsigned int linenr;

static __dead void error(void)
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

void inet_read_conf(void)
{
	int ifno, enable;
	struct stat st;

	{ static int first= 1; 
		if (!first)
			panic(( "LWIP : read_conf: called a second time" ));
		first= 0;
#if 0
		*(u8_t *)0 = 0xcc;	/* INT 3 */
#endif
	}


	/* Open the configuration file. */
	if ((cfg_fd= open(PATH_INET_CONF, O_RDONLY)) == -1)
		fatal(PATH_INET_CONF);

	while (nextline()) {
		token(1);
		char drv_name[128];
		unsigned int instance;

		if (strncmp(word, "eth", 3) == 0) {

			ifno = strtol(word+3, NULL, 10);
			token(1);
#if 1
			strncpy(drv_name, word, 128);
#else
			sprintf(drv_name, "%s_debug", word);
#endif
			token(1);
			instance = strtol(word, NULL, 10);
		} else {
			printf("inet: Unknown device '%s'\n", word);
			error();
		}

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

		nic_assign_driver("eth", ifno, drv_name, instance, ifdefault == ifno);
	}

	if (ifdefault == -1) {
		printf("inet: No networks or no default network defined\n");
		exit(1);
	}

	/* Set umask 0 so we can creat mode 666 devices. */
	(void) umask(0);

	/* See what the device number of /dev/ip is.  That's what we
	 * used last time for the network devices, so we keep doing so.
	 */
	if (stat("/dev/ip", &st) < 0) fatal("/dev/ip");
	ip_dev= st.st_rdev;

	/* create protocol devices */
	check_mknod("/dev/ip", 0600, SOCK_TYPE_IP);
	check_mknod("/dev/tcp", 0666, SOCK_TYPE_TCP);
	check_mknod("/dev/udp", 0666, SOCK_TYPE_UDP);

	/*
	 * create hw devices, to configure ip we need also ip devices for each
	 */
	check_mknod("/dev/ip0", 0600, SOCK_TYPES + 0);
	check_mknod("/dev/eth0", 0600, SOCK_TYPES + 0);

	check_mknod("/dev/ip1", 0600, SOCK_TYPES + 1);
	check_mknod("/dev/eth1", 0600, SOCK_TYPES + 1);

	check_mknod("/dev/ip2", 0600, SOCK_TYPES + 2);
	check_mknod("/dev/eth2", 0600, SOCK_TYPES + 2);

	check_mknod("/dev/ip3", 0600, SOCK_TYPES + 3);
	check_mknod("/dev/eth3", 0600, SOCK_TYPES + 3);

	check_mknod("/dev/ip4", 0600, SOCK_TYPES + 4);
	check_mknod("/dev/eth4", 0600, SOCK_TYPES + 4);

}
