/*	rget 2.6 - remote pipe				Author: Kees J. Bot
 *								20 Mar 1989
 *
 * here$ ... | rput key			there$ rget -h here key | ...
 * here$ rput key command ...		there$ rget -h here key command ...
 *
 * (Once my first try at network programming, completely reworked by now.)
 */
#define nil ((void*)0)
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#if __minix
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <netdb.h>
#include <net/gen/socket.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>
#include <net/netlib.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

static char *name;
static int iflag, oflag, hflag, lflag, cflag;	/* -iolch? */
static char *host;				/* Argument to -h. */
static struct hostent *hent;			/* gethostbyname(host) */
static char *key;				/* key (port) */
static char **cmdv;				/* command [arg ...] */

static void fatal(const char *label)
{
    int err= errno;

    fprintf(stderr, "%s: %s: %s\n", name, label, strerror(err));
    exit(1);
}

static unsigned name2port(char *n)
{
    char *end;
    unsigned port;

    port= strtoul(n, &end, 0);
    if (end == n || *end != 0) {
	port= 1;
	while (*n != 0) port *= (*n++ & 0xFF);
	port |= 0x8000;
    }
    return htons(port & 0xFFFF);
}

static void usage(void)
{
    fprintf(stderr,
	"Usage: %s [-lcio] [-h host] key [command [arg ...]]\n"
	"\t-l: Open TCP socket and listen (default for rput)\n"
	"\t-c: Connect to a remote TCP socket (default for rget)\n"
	"\t-i: Tie standard input to the TCP stream (default for rget)\n"
	"\t-o: Tie standard output to the TCP stream (default for rput)\n"
	"\t-io: Bidirectional!\n"
	"\tkey: A word to hash into a port number, or simply a port number\n",
	name);
    exit(1);
}

int main(int argc, char **argv)
{
    int i, s;

    if ((name= strrchr(argv[0], '/')) == nil) name= argv[0]; else name++;

    if (strcmp(name, "rget") != 0 && strcmp(name, "rput") != 0) {
	fprintf(stderr, "Don't know what to do if you call me '%s'\n", name);
	exit(1);
    }

    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++]+1;

	if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

	while (*opt != 0) switch (*opt++) {
	case 'l':	lflag= 1;	break;
	case 'c':	cflag= 1;	break;
	case 'i':	iflag= 1;	break;
	case 'o':	oflag= 1;	break;
	case 'h':
	    hflag= 1;
	    if (*opt == 0) {
		if (i == argc) usage();
		opt= argv[i++];
	    }
	    host= opt;
	    opt= "";
	    break;
	default:	usage();	break;
	}
    }

    if (i == argc) usage();
    key= argv[i++];
    cmdv= argv + i;

    /* Defaults. */
    if (!lflag && !cflag) {
	if (name[1] == 'p') lflag= 1;
	if (name[1] == 'g') cflag= 1;
    }
    if (!iflag && !oflag) {
	if (name[1] == 'g') iflag= 1;
	if (name[1] == 'p') oflag= 1;
    }

    /* Constraints. */
    if (lflag && cflag) {
	fprintf(stderr, "%s: -c and -l don't mix\n", name);
	usage();
    }
    if (cflag && !hflag) {
	fprintf(stderr, "%s: -c requires a host name given with -h\n", name);
	usage();
    }
    if (lflag && hflag) {
	fprintf(stderr, "%s: -l does not require a host name given with -h\n",
	    name);
	usage();
    }
    if (iflag && oflag && cmdv[0] == nil) {
	fprintf(stderr, "%s: -io requires that a command is given\n", name);
	usage();
    }

    if (hflag) {
	if ((hent= gethostbyname(host)) == nil) {
	    fprintf(stderr, "%s: %s: Name lookup failed\n", name, host);
	    exit(1);
	}
    }

    s= -1;
    if (lflag) {
	/* We need to listen and wait.  (We're "rput", most likely.) */
#if __minix
	char *tcp_device;
	struct nwio_tcpconf tcpconf;
	struct nwio_tcpcl tcplistenopt;

	if ((tcp_device= getenv("TCP_DEVICE")) == nil) tcp_device= "/dev/tcp";
	if ((s= open(tcp_device, O_RDWR)) < 0) fatal(tcp_device);

	tcpconf.nwtc_flags=
	    NWTC_EXCL | NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
	tcpconf.nwtc_locport= name2port(key);
	if (ioctl(s, NWIOSTCPCONF, &tcpconf) < 0) fatal("NWIOSTCPCONF");

	tcplistenopt.nwtcl_flags= 0;
	if (ioctl(s, NWIOTCPLISTEN, &tcplistenopt) < 0) fatal("NWIOTCPLISTEN");
#else
	int sa;
	struct sockaddr_in channel;
	static int on= 1;

	if ((s= socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0) fatal("socket()");

	(void) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
								sizeof(on));
	memset(&channel, 0, sizeof(channel));
	channel.sin_family= AF_INET;
	channel.sin_addr.s_addr= htonl(INADDR_ANY);
	channel.sin_port= name2port(key);
	if (bind(s, (struct sockaddr *) &channel, sizeof(channel)) < 0)
	    fatal("bind()");

	if (listen(s, 0) < 0) fatal("listen()");

	if ((sa= accept(s, nil, nil)) < 0) fatal("accept()");
	close(s);
	s= sa;
#endif
    }

    if (cflag) {
	/* Connect to the remote end.  (We're "rget", most likely.) */
#if __minix
	int n;
	char *tcp_device;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpconnopt;

	if ((tcp_device= getenv("TCP_DEVICE")) == nil) tcp_device= "/dev/tcp";

	n=60;
	for (;;) {
	    if ((s= open(tcp_device, O_RDWR)) < 0) fatal(tcp_device);

	    tcpconf.nwtc_flags= NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	    memcpy(&tcpconf.nwtc_remaddr, hent->h_addr,
		sizeof(tcpconf.nwtc_remaddr));
	    tcpconf.nwtc_remport= name2port(key);
	    if (ioctl(s, NWIOSTCPCONF, &tcpconf) < 0) fatal("NWIOSTCPCONF");

	    tcpconnopt.nwtcl_flags= 0;
	    if (ioctl(s, NWIOTCPCONN, &tcpconnopt) == 0) break;

	    if (--n > 0) sleep(2); else fatal("NWIOTCPCONN");
	    close(s);
	}
#else
	int n;
	struct sockaddr_in channel;

	n=60;
	for (;;) {
	    if ((s= socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		fatal("socket()");

	    memset(&channel, 0, sizeof(channel));
	    channel.sin_family= AF_INET;
	    memcpy(&channel.sin_addr.s_addr, hent->h_addr,
		sizeof(channel.sin_addr.s_addr));
	    channel.sin_port= name2port(key);
	    if (connect(s, (struct sockaddr *) &channel,
			    sizeof(channel)) >= 0) break;

	    if (--n > 0) sleep(2); else fatal("connect()");
	    close(s);
	}
#endif
    }

    if (cmdv[0] != nil) {
	/* A command is given, so execute it with standard input (rget),
	 * standard output (rput) or both (-io) tied to the TCP stream.
	 */
	if (iflag) dup2(s, 0);
	if (oflag) dup2(s, 1);
	close(s);

	execvp(cmdv[0], cmdv);
	fatal(cmdv[0]);
    } else {
	/* Without a command we have to copy bytes ourselves, probably to or
	 * from a command that is connected to us with a pipe.  (The original
	 * function of rput/rget, a remote pipe.)
	 */
	int fi, fo;
	int n;
	char buf[8192];

	if (iflag) {
	    fi= s;
	    fo= 1;
	} else {
	    fi= 0;
	    fo= s;
	}

	while ((n= read(fi, buf, sizeof(buf))) > 0) {
	    char *bp= buf;

	    while (n > 0) {
		int r;

		if ((r= write(fo, bp, n)) <= 0) {
		    if (r == 0) {
			fprintf(stderr, "%s: write(): Unexpected EOF\n", name);
			exit(1);
		    }
		    fatal("write()");
		}
		bp+= r;
		n-= r;
	    }
	}
	if (n < 0) fatal("read()");
    }
    return 0;
}
