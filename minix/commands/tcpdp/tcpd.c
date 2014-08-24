/*
tcpd.c
*/

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <minix/config.h>
#include <minix/paths.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <netdb.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

/* This program can be compiled to be paranoid, i.e. check incoming connection
 * according to an access file, or to trust anyone.  The much smaller "trust
 * 'em" binary will call the paranoid version if the access file exists.
 */

static char *arg0, *service;
static unsigned nchildren;

static void report(const char *label)
{
    int err= errno;

    fprintf(stderr, "%s %s: %s: %s\n", arg0, service, label, strerror(err));
    errno= err;
}

static void sigchld(int sig)
{
    while (waitpid(0, NULL, WNOHANG) > 0) {
	if (nchildren > 0) nchildren--;
    }
}

static void release(int *fd)
{
    if (*fd != -1) {
	close(*fd);
	*fd= -1;
    }
}

static void usage(void)
{
    fprintf(stderr,
	"Usage: %s [-d] [-m maxclients] service program [arg ...]\n",
	arg0);
    exit(1);
}

int main(int argc, char **argv)
{
    tcpport_t port;
    int last_failed = 0;
    struct nwio_tcpcl tcplistenopt;
    struct nwio_tcpconf tcpconf;
    struct nwio_tcpopt tcpopt;
    char *tcp_device;
    struct servent *servent;
    int tcp_fd, client_fd, r;
    int pfd[2];
    unsigned stall= 0;
    struct sigaction sa;
    sigset_t chldmask, chldunmask, oldmask;
    char **progv;

#if !PARANOID
#   define debug 0
#   define max_children ((unsigned) -1)
    arg0= argv[0];

    /* Switch to the paranoid version of me if there are flags, or if
     * there is an access file.
     */
    if (argv[1][0] == '-' || access(_PATH_SERVACCES, F_OK) == 0) {
	execv("/usr/bin/tcpdp", argv);
	report("tcpdp");
	exit(1);
    }
    if (argc < 3) usage();
    service= argv[1];
    progv= argv+2;

#else /* PARANOID */
    int debug, i;
    unsigned max_children;

    arg0= argv[0];
    debug= 0;
    max_children= -1;
    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++] + 1;
	unsigned long m;
	char *end;

	if (*opt == '-' && opt[1] == 0) break;	/* -- */

	while (*opt != 0) switch (*opt++) {
	case 'd':
	    debug= 1;
	    break;
	case 'm':
	    if (*opt == 0) {
		if (i == argc) usage();
		opt= argv[i++];
	    }
	    m= strtoul(opt, &end, 10);
	    if (m <= 0 || m > UINT_MAX || *end != 0) usage();
	    max_children= m;
	    opt= "";
	    break;
	default:
	    usage();
	}
    }
    service= argv[i++];
    progv= argv+i;
    if (i >= argc) usage();
#endif

    /* The interface to start the service on. */
    if ((tcp_device= getenv("TCP_DEVICE")) == NULL) tcp_device= TCP_DEVICE;

    /* Let SIGCHLD interrupt whatever I'm doing. */
    sigemptyset(&chldmask);
    sigaddset(&chldmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &chldmask, &oldmask);
    chldunmask= oldmask;
    sigdelset(&chldunmask, SIGCHLD);
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigchld;
    sigaction(SIGCHLD, &sa, NULL);

    /* Open a socket to the service I'm to serve. */
    if ((servent= getservbyname(service, "tcp")) == NULL) {
	unsigned long p;
	char *end;

	p= strtoul(service, &end, 0);
	if (p <= 0 || p > 0xFFFF || *end != 0) {
	    fprintf(stderr, "%s: %s: Unknown service\n",
		arg0, service);
	    exit(1);
	}
	port= htons((tcpport_t) p);
    } else {
	port= servent->s_port;

	if (debug)
	{
	    fprintf(stderr, "%s %s: listening to port %u\n",
		arg0, service, ntohs(port));
	}
    }

    /* No client yet. */
    client_fd= -1;

    while (1) {
	if ((tcp_fd= open(tcp_device, O_RDWR)) < 0) {
	    report(tcp_device);
#if 0
	    if (errno == ENOENT || errno == ENODEV
			    || errno == ENXIO) {
		exit(1);
	    }
#endif
	    last_failed = 1;
	    goto bad;
	}
	if(last_failed)
		fprintf(stderr, "%s %s: %s: Ok\n",
			arg0, service, tcp_device);
	last_failed = 0;

	tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
	tcpconf.nwtc_locport= port;

	if (ioctl(tcp_fd, NWIOSTCPCONF, &tcpconf) < 0) {
	    report("Can't configure TCP channel");
	    exit(1);
	}

	tcpopt.nwto_flags= NWTO_DEL_RST;

	if (ioctl(tcp_fd, NWIOSTCPOPT, &tcpopt) < 0) {
	    report("Can't set TCP options");
	    exit(1);
	}

	if (client_fd != -1) {
	    /* We have a client, so start a server for it. */

	    tcpopt.nwto_flags= 0;
	    (void) ioctl(client_fd, NWIOSTCPOPT, &tcpopt);

	    fflush(NULL);

	    /* Create a pipe to serve as an error indicator. */
	    if (pipe(pfd) < 0) {
		report("pipe");
		goto bad;
	    }
	    (void) fcntl(pfd[1], F_SETFD,
		    fcntl(pfd[1], F_GETFD) | FD_CLOEXEC);

	    /* Fork and exec. */
	    switch (fork()) {
	    case -1:
		report("fork");
		close(pfd[0]);
		close(pfd[1]);
		goto bad;
	    case 0:
		close(tcp_fd);
		close(pfd[0]);
#if PARANOID
		/* Check if access to this service allowed. */
		if (ioctl(client_fd, NWIOGTCPCONF, &tcpconf) == 0
		    && tcpconf.nwtc_remaddr != tcpconf.nwtc_locaddr
		    && !servxcheck(tcpconf.nwtc_remaddr, service, NULL)
		) {
		    exit(1);
		}
#endif
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		dup2(client_fd, 0);
		dup2(client_fd, 1);
		close(client_fd);
		execvp(progv[0], progv);
		report(progv[0]);
		write(pfd[1], &errno, sizeof(errno));
		exit(1);
	    default:
		nchildren++;
		release(&client_fd);
		close(pfd[1]);
		r= read(pfd[0], &errno, sizeof(errno));
		close(pfd[0]);
		if (r != 0) goto bad;
		break;
	    }
	}

	while (nchildren >= max_children) {
	    /* Too many clients, wait for one to die off. */
	    sigsuspend(&chldunmask);
	}

	/* Wait for a new connection. */
	sigprocmask(SIG_UNBLOCK, &chldmask, NULL);

	tcplistenopt.nwtcl_flags= 0;
	while (ioctl(tcp_fd, NWIOTCPLISTEN, &tcplistenopt) < 0) {
	    if (errno != EINTR) {
		if (errno != EAGAIN || debug) {
		    report("Unable to listen");
		}
		goto bad;
	    }
	}
	sigprocmask(SIG_BLOCK, &chldmask, NULL);

	/* We got a connection. */
	client_fd= tcp_fd;
	tcp_fd= -1;

	if (debug && ioctl(client_fd, NWIOGTCPCONF, &tcpconf) == 0) {
	    fprintf(stderr, "%s %s: Connection from %s:%u\n",
		arg0, service,
		inet_ntoa(tcpconf.nwtc_remaddr),
		ntohs(tcpconf.nwtc_remport));
	}
	/* All is well, no need to stall. */
	stall= 0;
	continue;

    bad:
	/* All is not well, release resources. */
	release(&tcp_fd);
	release(&client_fd);

	/* Wait a bit if this happens more than once. */
	if (stall != 0) {
	    if (debug) {
		fprintf(stderr, "%s %s: stalling %u second%s\n",
		    arg0, service,
		    stall, stall == 1 ? "" : "s");
	    }
	    sleep(stall);
	    stall <<= 1;
	} else {
	    stall= 1;
	}
    }
}
