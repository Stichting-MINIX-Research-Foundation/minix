#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "socklib.h"

/* 0 = check, 1 = generate source, 2 = generate CSV */
#define SOCKLIB_SWEEP_GENERATE 0

#if SOCKLIB_SWEEP_GENERATE
/* Link against minix/usr.bin/trace/error.o to make this work! */
const char *get_error_name(int err);

#if SOCKLIB_SWEEP_GENERATE == 2
static const char *statename[S_MAX] = {
	"S_NEW",
	"S_N_SHUT_R",
	"S_N_SHUT_W",
	"S_N_SHUT_RW",
	"S_BOUND",
	"S_LISTENING",
	"S_L_SHUT_R",
	"S_L_SHUT_W",
	"S_L_SHUT_RW",
	"S_CONNECTING",
	"S_C_SHUT_R",
	"S_C_SHUT_W",
	"S_C_SHUT_RW",
	"S_CONNECTED",
	"S_ACCEPTED",
	"S_SHUT_R",
	"S_SHUT_W",
	"S_SHUT_RW",
	"S_RSHUT_R",
	"S_RSHUT_W",
	"S_RSHUT_RW",
	"S_SHUT2_R",
	"S_SHUT2_W",
	"S_SHUT2_RW",
	"S_PRE_EOF",
	"S_AT_EOF",
	"S_POST_EOF",
	"S_PRE_SHUT_R",
	"S_EOF_SHUT_R",
	"S_POST_SHUT_R",
	"S_PRE_SHUT_W",
	"S_EOF_SHUT_W",
	"S_POST_SHUT_W",
	"S_PRE_SHUT_RW",
	"S_EOF_SHUT_RW",
	"S_POST_SHUT_RW",
	"S_PRE_RESET",
	"S_AT_RESET",
	"S_POST_RESET",
	"S_FAILED",
	"S_POST_FAILED",
};
#endif

static const char *callname[C_MAX] = {
	"C_ACCEPT",
	"C_BIND",
	"C_CONNECT",
	"C_GETPEERNAME",
	"C_GETSOCKNAME",
	"C_GETSOCKOPT_ERR",
	"C_GETSOCKOPT_KA",
	"C_GETSOCKOPT_RB",
	"C_IOCTL_NREAD",
	"C_LISTEN",
	"C_RECV",
	"C_RECVFROM",
	"C_SEND",
	"C_SENDTO",
	"C_SELECT_R",
	"C_SELECT_W",
	"C_SELECT_X",
	"C_SETSOCKOPT_BC",
	"C_SETSOCKOPT_KA",
	"C_SETSOCKOPT_L",
	"C_SETSOCKOPT_RA",
	"C_SHUTDOWN_R",
	"C_SHUTDOWN_RW",
	"C_SHUTDOWN_W",
};
#endif

static int socklib_sigpipe;

/*
 * Signal handler for SIGPIPE signals.
 */
static void
socklib_signal(int sig)
{

	if (sig != SIGPIPE) e(0);

	socklib_sigpipe++;
}

/*
 * The given socket file descriptor 'fd' has been set up in the desired state.
 * Perform the given call 'call' on it, possibly using local socket address
 * 'local_addr' (for binding) or remote socket address 'remote_addr' (for
 * connecting or to store resulting addresses), both of size 'addr_len'.
 * Return the result of the call, using a positive value if the call succeeded,
 * or a negated errno code if the call failed.
 */
int
socklib_sweep_call(enum call call, int fd, struct sockaddr * local_addr,
	struct sockaddr * remote_addr, socklen_t addr_len)
{
	char data[1];
	struct linger l;
	fd_set fd_set;
	struct timeval tv;
	socklen_t len;
	int i, r, fd2;

	fd2 = -1;

	switch (call) {
	case C_ACCEPT:
		r = accept(fd, remote_addr, &addr_len);

		if (r >= 0)
			fd2 = r;

		break;

	case C_BIND:
		r = bind(fd, local_addr, addr_len);

		break;

	case C_CONNECT:
		r = connect(fd, remote_addr, addr_len);

		break;

	case C_GETPEERNAME:
		r = getpeername(fd, remote_addr, &addr_len);

		break;

	case C_GETSOCKNAME:
		r = getsockname(fd, remote_addr, &addr_len);

		break;

	case C_GETSOCKOPT_ERR:
		len = sizeof(i);

		r = getsockopt(fd, SOL_SOCKET, SO_ERROR, &i, &len);

		/*
		 * We assume this call always succeeds, and test against the
		 * pending error.
		 */
		if (r != 0) e(0);
		if (i != 0) {
			r = -1;
			errno = i;
		}

		break;

	case C_GETSOCKOPT_KA:
		len = sizeof(i);

		r = getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &i, &len);

		break;

	case C_GETSOCKOPT_RB:
		len = sizeof(i);

		r = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &i, &len);

		break;

	case C_IOCTL_NREAD:
		r = ioctl(fd, FIONREAD, &i);

		/* On success, we test against the returned value here. */
		if (r == 0)
			r = i;

		break;

	case C_LISTEN:
		r = listen(fd, 1);

		break;

	case C_RECV:
		r = recv(fd, data, sizeof(data), 0);

		break;

	case C_RECVFROM:
		r = recvfrom(fd, data, sizeof(data), 0, remote_addr,
		    &addr_len);

		break;

	case C_SEND:
		data[0] = 0;

		r = send(fd, data, sizeof(data), 0);

		break;

	case C_SENDTO:
		data[0] = 0;

		r = sendto(fd, data, sizeof(data), 0, remote_addr, addr_len);

		break;

	case C_SETSOCKOPT_BC:
		i = 0;

		r = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &i, sizeof(i));

		break;

	case C_SETSOCKOPT_KA:
		i = 1;

		r = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &i, sizeof(i));

		break;

	case C_SETSOCKOPT_L:
		l.l_onoff = 1;
		l.l_linger = 0;

		r = setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));

		break;

	case C_SETSOCKOPT_RA:
		i = 1;

		r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

		break;

	case C_SELECT_R:
	case C_SELECT_W:
	case C_SELECT_X:
		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);

		tv.tv_sec = 0;
		tv.tv_usec = 0;

		r = select(fd + 1, (call == C_SELECT_R) ? &fd_set : NULL,
		    (call == C_SELECT_W) ? &fd_set : NULL,
		    (call == C_SELECT_X) ? &fd_set : NULL, &tv);

		break;

	case C_SHUTDOWN_R:
		r = shutdown(fd, SHUT_RD);

		break;

	case C_SHUTDOWN_W:
		r = shutdown(fd, SHUT_WR);

		break;

	case C_SHUTDOWN_RW:
		r = shutdown(fd, SHUT_RDWR);

		break;

	default:
		r = -1;
		errno = EINVAL;
		e(0);
	}

	if (r < -1) e(0);

	if (r == -1)
		r = -errno;

	if (fd2 >= 0 && close(fd2) != 0) e(0);

	return r;
}

/*
 * Perform a sweep of socket calls vs socket states, testing the outcomes
 * against provided tables or (if SOCKLIB_SWEEP_GENERATE is set) reporting on
 * the outcomes instead.  The caller must provide the following:
 *
 * - the socket domain, type, and protocol to test; these are simply forwarded
 *   to the callback function (see below);
 * - the set of S_ states to test, as array 'states' with 'nstates' elements;
 * - unless generating output, a matrix of expected results as 'results', which
 *   is actually a two-dimensional array with dimensions [C_MAX][nstates], with
 *   either positive call output or a negated call errno code in each cell;
 * - a callback function 'proc' that must set up a socket in the given state
 *   and pass it to socklib_sweep_call().
 *
 * The 'states' array allows each socket sweep test to support a different set
 * of states, because not every type of socket can be put in every possible
 * state.  All calls are always tried in each state, though.
 *
 * The sweep also tests for SIGPIPE generation, which assumes that all calls on
 * SOCK_STREAM sockets that return EPIPE, also raise a SIGPIPE signal, and that
 * no other SIGPIPE signal is ever raised otherwise.
 *
 * Standard e() error throwing is used for set-up and result mismatches.
 */
void
socklib_sweep(int domain, int type, int protocol, const enum state * states,
	unsigned int nstates, const int * results, int (* proc)(int domain,
	int type, int protocol, enum state, enum call))
{
	struct sigaction act, oact;
	enum state state;
	enum call call;
#if SOCKLIB_SWEEP_GENERATE
	const char *name;
	int res, *nresults;
#else
	int res, exp;
#endif

	memset(&act, 0, sizeof(act));
	act.sa_handler = socklib_signal;
	if (sigaction(SIGPIPE, &act, &oact) != 0) e(0);

#if SOCKLIB_SWEEP_GENERATE
	if ((nresults = malloc(nstates * C_MAX)) == NULL) e(0);
#endif

	for (state = 0; state < nstates; state++) {
		for (call = 0; call < C_MAX; call++) {
			socklib_sigpipe = 0;

			res = proc(domain, type, protocol, states[state],
			    call);

			/*
			 * If the result was EPIPE and this is a stream-type
			 * socket, we must have received exactly one SIGPIPE
			 * signal.  Otherwise, we must not have received one.
			 * Note that technically, the SIGPIPE could arrive
			 * sometime after this check, but with regular system
			 * service scheduling that will never happen.
			 */
			if (socklib_sigpipe !=
			    (res == -EPIPE && type == SOCK_STREAM)) e(0);

#if SOCKLIB_SWEEP_GENERATE
			nresults[call * nstates + state] = res;
#else
			exp = results[call * nstates + state];

			if (res != exp) {
				printf("FAIL state %d call %d res %d exp %d\n",
				    state, call, res, exp);
				e(0);
			}
#endif
		}
	}

	if (sigaction(SIGPIPE, &oact, NULL) != 0) e(0);

#if SOCKLIB_SWEEP_GENERATE
#if SOCKLIB_SWEEP_GENERATE == 1
	/*
	 * Generate a table in C form, ready to be pasted into test source.
	 * Obviously, generated results should be hand-checked carefully before
	 * being pasted into a test.  Arguably these tables should be hand-made
	 * for maximum scrutiny, but I already checked the results from the
	 * CSV form (#define SOCKLIB_SWEEP_GENERATE 2) and have no desire for
	 * RSI -dcvmoole
	 */
	printf("\nstatic const int X_results[][__arraycount(X_states)] = {\n");
	for (call = 0; call < C_MAX; call++) {
		if ((name = callname[call]) == NULL) e(0);
		printf("\t[%s]%s%s%s= {", name,
		    (strlen(name) <= 21) ? "\t" : "",
		    (strlen(name) <= 13) ? "\t" : "",
		    (strlen(name) <= 5) ? "\t" : "");
		for (state = 0; state < nstates; state++) {
			if (state % 4 == 0)
				printf("\n\t\t");
			res = nresults[call * nstates + state];
			name = (res < 0) ? get_error_name(-res) : NULL;
			if (name != NULL) {
				printf("-%s,", name);
				if ((state + 1) % 4 != 0 &&
				    state < nstates - 1)
					printf("%s%s",
					    (strlen(name) <= 13) ? "\t" : "",
					    (strlen(name) <= 5) ? "\t" : "");
			} else {
				printf("%d,", res);
				if ((state + 1) % 4 != 0 &&
				    state < nstates - 1)
					printf("\t\t");
			}
		}
		printf("\n\t},\n");
	}
	printf("};\n");
#elif SOCKLIB_SWEEP_GENERATE == 2
	/* Generate table in CSV form. */
	printf("\n");
	for (state = 0; state < nstates; state++)
		printf(",%s", statename[states[state]] + 2);
	for (call = 0; call < C_MAX; call++) {
		printf("\n%s", callname[call] + 2);
		for (state = 0; state < nstates; state++) {
			res = nresults[call * nstates + state];
			name = (res < 0) ? get_error_name(-res) : NULL;
			if (name != NULL)
				printf(",%s", name);
			else
				printf(",%d", res);
		}
	}
	printf("\n");
#endif

	free(nresults);
#endif
}

/*
 * Test for large sends and receives on stream sockets with MSG_WAITALL.
 */
void
socklib_large_transfers(int fd[2])
{
	char *buf;
	pid_t pid;
	int i, status;

#define LARGE_BUF	(4096*1024)

	if ((buf = malloc(LARGE_BUF)) == NULL) e(0);
	memset(buf, 0, LARGE_BUF);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[0]) != 0) e(0);

		/* Part 1. */
		if (recv(fd[1], buf, LARGE_BUF, MSG_WAITALL) != LARGE_BUF)
			e(0);

		for (i = 0; i < LARGE_BUF; i++)
			if (buf[i] != (char)(i + (i >> 16))) e(0);

		if (recv(fd[1], buf, LARGE_BUF,
		    MSG_DONTWAIT | MSG_WAITALL) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);

		/* Part 2. */
		if (send(fd[1], buf, LARGE_BUF / 2, 0) != LARGE_BUF / 2) e(0);

		if (shutdown(fd[1], SHUT_WR) != 0) e(0);

		/* Part 3. */
		memset(buf, 'y', LARGE_BUF);

		if (recv(fd[1], buf, LARGE_BUF, MSG_WAITALL) != LARGE_BUF - 1)
			e(0);

		for (i = 0; i < LARGE_BUF - 1; i++)
			if (buf[i] != (char)(i + (i >> 16))) e(0);
		if (buf[LARGE_BUF - 1] != 'y') e(0);

		if (recv(fd[1], buf, LARGE_BUF, MSG_WAITALL) != 0) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd[1]) != 0) e(0);

	/* Part 1: check that a large send fully arrives. */
	for (i = 0; i < LARGE_BUF; i++)
		buf[i] = (char)(i + (i >> 16));

	if (send(fd[0], buf, LARGE_BUF, 0) != LARGE_BUF) e(0);

	/* Part 2: check that remote shutdown terminates a partial receive. */
	memset(buf, 'x', LARGE_BUF);

	if (recv(fd[0], buf, LARGE_BUF, MSG_WAITALL) != LARGE_BUF / 2) e(0);

	for (i = 0; i < LARGE_BUF / 2; i++)
		if (buf[i] != (char)(i + (i >> 16))) e(0);
	for (; i < LARGE_BUF; i++)
		if (buf[i] != 'x') e(0);

	if (recv(fd[0], buf, LARGE_BUF, MSG_WAITALL) != 0) e(0);

	/* Part 3: check that remote close terminates a partial receive. */
	for (i = 0; i < LARGE_BUF; i++)
		buf[i] = (char)(i + (i >> 16));

	if (send(fd[0], buf, LARGE_BUF - 1, 0) != LARGE_BUF - 1) e(0);

	if (close(fd[0]) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	free(buf);
}

#define PRINT_STATS	0

/*
 * A randomized producer-consumer test for stream sockets.  As part of this,
 * we also perform very basic bulk functionality tests of FIONREAD, MSG_PEEK,
 * MSG_DONTWAIT, and MSG_WAITALL.
 */
void
socklib_producer_consumer(int fd[2])
{
	char *buf;
	time_t t;
	socklen_t len, size, off;
	ssize_t r;
	pid_t pid;
	int i, rcvlen, status, exp, flags, num, stat[3] = { 0, 0, 0 };

	len = sizeof(rcvlen);
	if (getsockopt(fd[0], SOL_SOCKET, SO_RCVBUF, &rcvlen, &len) != 0) e(0);
	if (len != sizeof(rcvlen)) e(0);

	size = rcvlen * 3;

	if ((buf = malloc(size)) == NULL) e(0);

	t = time(NULL);

	/*
	 * We vary small versus large (random) send and receive sizes,
	 * splitting the entire transfer in four phases along those lines.
	 *
	 * In theory, the use of an extra system call, the use of MSG_PEEK, and
	 * the fact that without MSG_WAITALL a receive call may return any
	 * partial result, all contribute to the expectation that the consumer
	 * side will fall behind the producer.  In order to test both filling
	 * and draining the receive queue, we use a somewhat larger small
	 * receive size for the consumer size (up to 256 bytes rather than 64)
	 * during each half of the four phases.  The effectiveness of these
	 * numbers can be verified with statistics (disabled by default).
	 */
#define TRANSFER_SIZE	(16 * 1024 * 1024)

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[0]) != 0) e(0);

		srand48(t + 1);

		for (off = 0; off < TRANSFER_SIZE; ) {
			if (off < TRANSFER_SIZE / 2)
				len = lrand48() %
				    ((off / (TRANSFER_SIZE / 8) % 2) ? 64 :
				    256);
			else
				len = lrand48() % size;

			num = lrand48() % 16;
			flags = 0;
			if (num & 1) flags |= MSG_PEEK;
			if (num & 2) flags |= MSG_WAITALL;
			if (num & 4) flags |= MSG_DONTWAIT;
			if (num & 8) {
				/*
				 * Obviously there are race conditions here but
				 * the returned number should be a lower bound.
				 */
				if (ioctl(fd[1], FIONREAD, &exp) != 0) e(0);
				if (exp < 0 || exp > rcvlen) e(0);
			} else
				exp = -1;

			stat[0]++;

			if ((r = recv(fd[1], buf, len, flags)) == -1) {
				if (errno != EWOULDBLOCK) e(0);
				if (exp > 0) e(0);

				stat[2]++;
				continue;
			}

			if (r < len) {
				stat[1]++;

				if (exp > r) e(0);
			}

			for (i = 0; i < r; i++)
				if (buf[i] != (char)((off + i) +
				    ((off + i) >> 16))) e(0);

			if (!(flags & MSG_PEEK)) {
				off += r;

				if ((flags & (MSG_DONTWAIT | MSG_WAITALL)) ==
				    MSG_WAITALL && r != len &&
				    off < TRANSFER_SIZE) e(0);
			}
		}

#if PRINT_STATS
		/*
		 * The second and third numbers should ideally be a large but
		 * non-dominating fraction of the first one.
		 */
		printf("RECV: total %d short %d again %d\n",
		    stat[0], stat[1], stat[2]);
#endif

		if (close(fd[1]) != 0) e(0);
		exit(errct);
	case -1:
		e(0);
	}

	if (close(fd[1]) != 0) e(0);

	srand48(t);

	for (off = 0; off < TRANSFER_SIZE; ) {
		if (off < TRANSFER_SIZE / 4 ||
		    (off >= TRANSFER_SIZE / 2 && off < TRANSFER_SIZE * 3 / 4))
			len = lrand48() % 64;
		else
			len = lrand48() % size;

		if (len > TRANSFER_SIZE - off)
			len = TRANSFER_SIZE - off;

		for (i = 0; i < len; i++)
			buf[i] = (off + i) + ((off + i) >> 16);

		flags = (lrand48() % 2) ? MSG_DONTWAIT : 0;

		stat[0]++;

		r = send(fd[0], buf, len, flags);

		if (r != len) {
			if (r > (ssize_t)len) e(0);
			if (!(flags & MSG_DONTWAIT)) e(0);
			if (r == -1) {
				if (errno != EWOULDBLOCK) e(0);
				r = 0;

				stat[2]++;
			} else
				stat[1]++;
		}

		if (off / (TRANSFER_SIZE / 4) !=
		    (off + r) / (TRANSFER_SIZE / 4))
			sleep(1);

		off += r;
	}

#if PRINT_STATS
	/*
	 * The second and third numbers should ideally be a large but non-
	 * dominating fraction of the first one.
	 */
	printf("SEND: total %d short %d again %d\n",
	    stat[0], stat[1], stat[2]);
#endif

	free(buf);

	if (close(fd[0]) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);
}

/*
 * Signal handler which just needs to exist, so that invoking it will interrupt
 * an ongoing system call.
 */
static void
socklib_got_signal(int sig __unused)
{

	/* Nothing. */
}

/*
 * Test for receiving on stream sockets.  The quick summary here is that
 * recv(MSG_WAITALL) should keep suspending until as many bytes as requested
 * are also received (or the call is interrupted, or no more can possibly be
 * received - the meaning of the latter depends on the domain), and,
 * SO_RCVLOWAT acts as an admission test for the receive: nothing is received
 * until there are at least as many bytes are available in the receive buffer
 * as the low receive watermark, or the whole receive request length, whichever
 * is smaller. In addition, select(2) should use the same threshold.
 */
#define MAX_BYTES	2	/* set to 3 for slightly better(?) testing */
#define USLEEP_TIME	250000	/* increase on wimpy platforms if needed */

static void
socklib_stream_recv_sub(int (* socket_pair)(int, int, int, int *), int domain,
	int type, int idata, int istate, int rlowat, int len, int bits,
	int act, int (* break_recv)(int, const char *, size_t))
{
	const char *data = "ABCDE";	/* this limits MAX_BYTES to 3 */
	struct sigaction sa;
	struct timeval tv;
	fd_set fds;
	char buf[3];
	pid_t pid;
	int fd[2], val, flags, min, res, err;
	int pfd[2], edata, tstate, fl, status;

	if (socket_pair(domain, type, 0, fd) != 0) e(0);

	/*
	 * Set up the initial condition on the sockets.
	 */
	if (idata > 0)
		if (send(fd[1], data, idata, 0) != idata) e(0);

	switch (istate) {
	case 0: break;
	case 1: if (shutdown(fd[0], SHUT_RD) != 0) e(0); break;
	case 2: if (shutdown(fd[1], SHUT_WR) != 0) e(0); break;
	case 3: if (close(fd[1]) != 0) e(0); break;
	}

	/* Set the low receive water mark. */
	if (setsockopt(fd[0], SOL_SOCKET, SO_RCVLOWAT, &rlowat,
	    sizeof(rlowat)) != 0) e(0);

	/* SO_RCVLOWAT is always bounded by the actual receive length. */
	min = MIN(len, rlowat);

	/*
	 * Do a quick select test to see if its result indeed matches whether
	 * the available data in the receive buffer meets the threshold.
	 */
	FD_ZERO(&fds);
	FD_SET(fd[0], &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	res = select(fd[0] + 1, &fds, NULL, NULL, &tv);
	if (res < 0 || res > 1) e(0);
	if (res != (idata >= rlowat || istate > 0)) e(0);
	if (res == 1 && !FD_ISSET(fd[0], &fds)) e(0);

	/* Also do a quick test for ioctl(FIONREAD). */
	if (ioctl(fd[0], FIONREAD, &val) != 0) e(0);
	if (val != ((istate != 1) ? idata : 0)) e(0);

	/* Translate the given bits to receive call flags. */
	flags = 0;
	if (bits & 1) flags |= MSG_PEEK;
	if (bits & 2) flags |= MSG_DONTWAIT;
	if (bits & 4) flags |= MSG_WAITALL;

	/*
	 * Cut short a whole lot of cases, to avoid the overhead of forking,
	 * namely when we know the call should return immediately.  This is
	 * the case when MSG_DONTWAIT is set, or if a termination condition has
	 * been raised, or if enough initial data are available to meet the
	 * conditions for the receive call.
	 */
	if ((flags & MSG_DONTWAIT) || istate > 0 || (idata >= min &&
	    ((flags & (MSG_PEEK | MSG_WAITALL)) != MSG_WAITALL ||
	    idata >= len))) {
		res = recv(fd[0], buf, len, flags);

		if (res == -1 && errno != EWOULDBLOCK) e(0);

		/*
		 * If the socket has been shutdown locally, we will never get
		 * anything but zero.  Otherwise, if we meet the SO_RCVLOWAT
		 * test, we should have received as much as was available and
		 * requested.  Otherwise, if the remote end has been shut down
		 * or closed, we expected to get any available data or
		 * otherwise EOF (implied with idata==0).  If none of these
		 * cases apply, we should have gotten EWOULDBLOCK.
		 */
		if (istate == 1) {
			if (res != 0) e(0);
		} else if (idata >= min) {
			if (res != MIN(len, idata)) e(0);
			if (strncmp(buf, data, res)) e(0);
		} else if (istate > 0) {
			if (res != idata) e(0);
			if (strncmp(buf, data, res)) e(0);
		} else
			if (res != -1) e(0);

		/* Early cleanup and return to avoid even more code clutter. */
		if (istate != 3 && close(fd[1]) != 0) e(0);
		if (close(fd[0]) != 0) e(0);

		return;
	}

	/*
	 * Now starts the interesting stuff: the receive call should now block,
	 * even though if we add MSG_DONTWAIT it may not return EWOULDBLOCK,
	 * because MSG_DONTWAIT overrides MSG_WAITALL.  As such, we can only
	 * test our expectations by actually letting the call block, in a child
	 * process, and waiting.  We do test as much of the above assumption as
	 * we can just for safety right here, but this is not a substitute for
	 * actually blocking even in these cases!
	 */
	if (!(flags & MSG_WAITALL)) {
		if (recv(fd[0], buf, len, flags | MSG_DONTWAIT) != -1) e(0);
		if (errno != EWOULDBLOCK) e(0);
	}

	/*
	 * If (act < 12), we send 0, 1, or 2 extra data bytes before forcing
	 * the receive call to terminate in one of four ways.
	 *
	 * If (act == 12), we use a signal to interrupt the receive call.
	 */
	if (act < 12) {
		edata = act % 3;
		tstate = act / 3;
	} else
		edata = tstate = 0;

	if (pipe2(pfd, O_NONBLOCK) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if (close(fd[1]) != 0) e(0);
		if (close(pfd[0]) != 0) e(0);

		if (act == 12) {
			memset(&sa, 0, sizeof(sa));
			sa.sa_handler = socklib_got_signal;
			if (sigaction(SIGUSR1, &sa, NULL) != 0) e(0);
		}

		res = recv(fd[0], buf, len, flags);
		err = errno;

		if (write(pfd[1], &res, sizeof(res)) != sizeof(res)) e(0);
		if (write(pfd[1], &err, sizeof(err)) != sizeof(err)) e(0);

		if (res > 0 && strncmp(buf, data, res)) e(0);

		exit(errct);
	case -1:
		e(0);
	}

	if (close(pfd[1]) != 0) e(0);

	/*
	 * Allow the child to enter the blocking recv(2), and check the pipe
	 * to see if it is really blocked.
	 */
	if (usleep(USLEEP_TIME) != 0) e(0);

	if (read(pfd[0], buf, 1) != -1) e(0);
	if (errno != EAGAIN) e(0);

	if (edata > 0) {
		if (send(fd[1], &data[idata], edata, 0) != edata) e(0);

		/*
		 * The threshold for the receive is now met if both the minimum
		 * is met and MSG_WAITALL was not set (or overridden by
		 * MSG_PEEK) or the entire request has been satisfied.
		 */
		if (idata + edata >= min &&
		    ((flags & (MSG_PEEK | MSG_WAITALL)) != MSG_WAITALL ||
		    idata + edata >= len)) {
			if ((fl = fcntl(pfd[0], F_GETFL, 0)) == -1) e(0);
			if (fcntl(pfd[0], F_SETFL, fl & ~O_NONBLOCK) != 0)
			    e(0);

			if (read(pfd[0], &res, sizeof(res)) != sizeof(res))
			    e(0);
			if (read(pfd[0], &err, sizeof(err)) != sizeof(err))
			    e(0);

			if (res != MIN(idata + edata, len)) e(0);

			/* Bail out. */
			goto cleanup;
		}

		/* Sleep and test once more. */
		if (usleep(USLEEP_TIME) != 0) e(0);

		if (read(pfd[0], buf, 1) != -1) e(0);
		if (errno != EAGAIN) e(0);
	}

	if (act < 12) {
		/*
		 * Now test various ways to terminate the receive call.
		 */
		switch (tstate) {
		case 0: if (shutdown(fd[0], SHUT_RD) != 0) e(0); break;
		case 1: if (shutdown(fd[1], SHUT_WR) != 0) e(0); break;
		case 2: if (close(fd[1]) != 0) e(0); fd[1] = -1; break;
		case 3: fd[1] = break_recv(fd[1], data, strlen(data)); break;
		}
	} else
		if (kill(pid, SIGUSR1) != 0) e(0);

	if ((fl = fcntl(pfd[0], F_GETFL, 0)) == -1) e(0);
	if (fcntl(pfd[0], F_SETFL, fl & ~O_NONBLOCK) != 0) e(0);

	if (read(pfd[0], &res, sizeof(res)) != sizeof(res)) e(0);
	if (read(pfd[0], &err, sizeof(err)) != sizeof(err)) e(0);

	if (act < 12) {
		/*
		 * If there were any data we should have received them now;
		 * after all the receive minimum stops being relevant when
		 * another condition has been raised.  There is one exception:
		 * if the receive threshold was never met and we now shut down
		 * the socket for reading, EOF is acceptable as return value.
		 */
		if (tstate == 0 && idata + edata < min) {
			if (res != 0) e(0);
		} else if (idata + edata > 0) {
			if (res != MIN(idata + edata, len)) e(0);
		} else if (tstate == 3) {
			if (fd[1] == -1) {
				if (res != -1) e(0);
				if (err != ECONNRESET) e(0);
			} else
				if (res != len) e(0);
		} else
			if (res != 0) e(0);
	} else {
		/*
		 * If the receive met the threshold before being interrupted,
		 * we should have received at least something.  Otherwise, the
		 * receive was never admitted and should just return EINTR.
		 */
		if (idata >= min) {
			if (res != MIN(idata, len)) e(0);
		} else {
			if (res != -1) e(0);
			if (err != EINTR) e(0);
		}
	}

cleanup:
	if (close(pfd[0]) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	if (fd[1] != -1 && close(fd[1]) != 0) e(0);
	if (close(fd[0]) != 0) e(0);
}

/*
 * Test for receiving on stream sockets.  In particular, test SO_RCVLOWAT,
 * MSG_PEEK, MSG_DONTWAIT, and MSG_WAITALL.
 */
void
socklib_stream_recv(int (* socket_pair)(int, int, int, int *), int domain,
	int type, int (* break_recv)(int, const char *, size_t))
{
	int idata, istate, rlowat, len, bits, act;

	/* Insanity. */
	for (idata = 0; idata <= MAX_BYTES; idata++)
		for (istate = 0; istate <= 3; istate++)
			for (rlowat = 1; rlowat <= MAX_BYTES; rlowat++)
				for (len = 1; len <= MAX_BYTES; len++)
					for (bits = 0; bits < 8; bits++)
						for (act = 0; act <= 12; act++)
							socklib_stream_recv_sub
							    (socket_pair,
							    domain, type,
							    idata, istate,
							    rlowat, len, bits,
							    act, break_recv);
}
