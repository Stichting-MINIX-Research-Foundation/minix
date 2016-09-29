/* Tests for BPF devices (LWIP) - by D.C. van Moolenbroek */
/* This test needs to be run as root: opening BPF devices is root-only. */
/*
 * We do not attempt to test the BPF filter code here.  Such a test is better
 * done through standardized tests and with direct use of the filter code.
 * The current BPF filter implementation has been run through the FreeBSD
 * BPF filter regression tests (from their tools/regression/bpf/bpf_filter), of
 * which only the last test (0084 - "Check very long BPF program") failed due
 * to our lower and strictly enforced BPF_MAXINSNS value.  Future modifications
 * of the BPF filter code should be tested against at least that test set.
 */
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_ether.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>

#include "common.h"

#define ITERATIONS	2

#define LOOPBACK_IFNAME	"lo0"

#define TEST_PORT_A	12345
#define TEST_PORT_B	12346

#define SLEEP_TIME	250000	/* (us) - increases may require code changes */

#define NONROOT_USER	"bin"	/* name of any unprivileged user */

#ifdef NO_INET6
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
#endif /* NO_INET6 */

static unsigned int got_signal;

/*
 * Signal handler.
 */
static void
test94_signal(int sig)
{

	if (sig != SIGUSR1) e(0);

	got_signal++;
}

/*
 * Send UDP packets on the given socket 'fd' so as to fill up a BPF store
 * buffer of size 'size' exactly.  The provided buffer 'buf' may be used for
 * packet generation and is at least of 'size' bytes.  Return the number of
 * packets sent.
 */
static uint32_t
test94_fill_exact(int fd, uint8_t * buf, size_t size, uint32_t seq)
{
	size_t hdrlen, len;

	hdrlen = BPF_WORDALIGN(sizeof(struct bpf_hdr)) + sizeof(struct ip) +
	    sizeof(struct udphdr) + sizeof(seq);

	for (len = 16; len <= hdrlen; len <<= 1);
	if (len > size) e(0);

	hdrlen = BPF_WORDALIGN(hdrlen - sizeof(seq));

	for (; size > 0; seq++) {
		memset(buf, 'Y', len - hdrlen);
		if (len - hdrlen > sizeof(seq))
			buf[sizeof(seq)] = 'X';
		buf[len - hdrlen - 1] = 'Z';
		memcpy(buf, &seq, sizeof(seq));

		if (write(fd, buf, len - hdrlen) != len - hdrlen) e(0);

		size -= len;
	}

	return seq;
}

/*
 * Send UDP packets on the given socket 'fd' so as to fill up at least a BPF
 * store buffer of size 'size', with at least one more packet being sent.  The
 * provided buffer 'buf' may be used for packet generation and is at least of
 * 'size' bytes.
 */
static void
test94_fill_random(int fd, uint8_t * buf, size_t size)
{
	size_t hdrlen, len;
	ssize_t left;
	uint32_t seq;

	hdrlen = BPF_WORDALIGN(BPF_WORDALIGN(sizeof(struct bpf_hdr)) +
	    sizeof(struct ip) + sizeof(struct udphdr));

	/* Even if we fill the buffer exactly, we send one more packet. */
	for (left = (ssize_t)size, seq = 1; left >= 0; seq++) {
		len = hdrlen + sizeof(seq) + lrand48() % (size / 10);

		memset(buf, 'Y', len - hdrlen);
		if (len - hdrlen > sizeof(seq))
			buf[sizeof(seq)] = 'X';
		buf[len - hdrlen - 1] = 'Z';
		memcpy(buf, &seq, sizeof(seq));

		if (write(fd, buf, len - hdrlen) != len - hdrlen) e(0);

		left -= BPF_WORDALIGN(len);
	}
}

/*
 * Send a UDP packet with a specific size of 'size' bytes and sequence number
 * 'seq' on socket 'fd', using 'buf' as scratch buffer.
 */
static void
test94_add_specific(int fd, uint8_t * buf, size_t size, uint32_t seq)
{

	size += sizeof(seq);

	memset(buf, 'Y', size);
	if (size > sizeof(seq))
		buf[sizeof(seq)] = 'X';
	buf[size - 1] = 'Z';
	memcpy(buf, &seq, sizeof(seq));

	if (write(fd, buf, size) != size) e(0);
}

/*
 * Send a randomly sized, relatively small UDP packet on the given socket 'fd',
 * using sequence number 'seq'.  The buffer 'buf' may be used as scratch buffer
 * which is at most 'size' bytes--the same size as the total BPF buffer.
 */
static void
test94_add_random(int fd, uint8_t * buf, size_t size, uint32_t seq)
{

	test94_add_specific(fd, buf, lrand48() % (size / 10), seq);
}

/*
 * Check whether the packet in 'buf' of 'caplen' captured bytes out of
 * 'datalen' data bytes is one we sent.  If so, return an offset to the packet
 * data.  If not, return a negative value.
 */
static ssize_t
test94_check_pkt(uint8_t * buf, ssize_t caplen, ssize_t datalen)
{
	struct ip ip;
	struct udphdr uh;

	if (caplen < sizeof(ip))
		return -1;

	memcpy(&ip, buf, sizeof(ip));

	if (ip.ip_v != IPVERSION)
		return -1;
	if (ip.ip_hl != sizeof(ip) >> 2)
		return -1;
	if (ip.ip_p != IPPROTO_UDP)
		return -1;

	if (caplen - sizeof(ip) < sizeof(uh))
		return -1;

	memcpy(&uh, buf + sizeof(ip), sizeof(uh));

	if (uh.uh_sport != htons(TEST_PORT_A))
		return -1;
	if (uh.uh_dport != htons(TEST_PORT_B))
		return -1;

	if (datalen - sizeof(ip) != ntohs(uh.uh_ulen)) e(0);

	return sizeof(ip) + sizeof(uh);
}

/*
 * Check whether the capture in 'buf' of 'len' bytes looks like a valid set of
 * captured packets.  The valid packets start from sequence number 'seq'; the
 * next expected sequence number is returned.  If 'filtered' is set, there
 * should be no other packets in the capture; otherwise, other packets are
 * ignored.
 */
static uint32_t
test94_check(uint8_t * buf, ssize_t len, uint32_t seq, int filtered,
	uint32_t * caplen, uint32_t * datalen)
{
	struct bpf_hdr bh;
	ssize_t off;
	uint32_t nseq;

	while (len > 0) {
		/*
		 * We rely on the assumption that the last packet in the buffer
		 * is padded to alignment as well; if not, this check fails.
		 */
		if (len < BPF_WORDALIGN(sizeof(bh))) e(0);

		memcpy(&bh, buf, sizeof(bh));

		/*
		 * The timestamp fields should be filled in.  The tests that
		 * use this function do not set a capture length below the
		 * packet length.  The header must be exactly as large as we
		 * expect: no small-size tricks (as NetBSD uses) and no
		 * unexpected extra padding.
		 */
		if (bh.bh_tstamp.tv_sec == 0 && bh.bh_tstamp.tv_usec == 0)
		     e(0);
		if (caplen != NULL) {
			if (bh.bh_caplen != *caplen) e(0);
			if (bh.bh_datalen != *datalen) e(0);

			caplen++;
			datalen++;
		} else
			if (bh.bh_datalen != bh.bh_caplen) e(0);
		if (bh.bh_hdrlen != BPF_WORDALIGN(sizeof(bh))) e(0);

		if (bh.bh_hdrlen + BPF_WORDALIGN(bh.bh_caplen) > len) e(0);

		buf += bh.bh_hdrlen;
		len -= bh.bh_hdrlen;

		if ((off = test94_check_pkt(buf, bh.bh_caplen,
		    bh.bh_datalen)) < 0) {
			if (filtered) e(0);

			buf += BPF_WORDALIGN(bh.bh_caplen);
			len -= BPF_WORDALIGN(bh.bh_caplen);

			continue;
		}

		if (bh.bh_caplen < off + sizeof(seq)) e(0);

		memcpy(&nseq, &buf[off], sizeof(nseq));

		if (nseq != seq++) e(0);

		off += sizeof(seq);
		if (off < bh.bh_caplen) {
			/* If there is just one byte, it is 'Z'. */
			if (off < bh.bh_caplen && off < bh.bh_datalen - 1) {
				if (buf[off] != 'X') e(0);

				for (off++; off < bh.bh_caplen &&
				    off < bh.bh_datalen - 1; off++)
					if (buf[off] != 'Y') e(0);
			}
			if (off < bh.bh_caplen && off == bh.bh_datalen - 1 &&
			    buf[off] != 'Z') e(0);
		}

		buf += BPF_WORDALIGN(bh.bh_caplen);
		len -= BPF_WORDALIGN(bh.bh_caplen);
	}

	return seq;
}

/*
 * Filter program to ensure that the given (datalink-headerless) packet is an
 * IPv4 UDP packet from port 12345 to port 12346.  Important: the 'k' value of
 * the last instruction must be the accepted packet size, and is modified by
 * some of the tests further down!
 */
static struct bpf_insn test94_filter[] = {
	{ BPF_LD+BPF_B+BPF_ABS, 0, 0, 0 },	/* is this an IPv4 header? */
	{ BPF_ALU+BPF_RSH+BPF_K, 0, 0, 4 },
	{ BPF_JMP+BPF_JEQ+BPF_K, 0, 7, 4 },
	{ BPF_LD+BPF_B+BPF_ABS, 0, 0, 9 },	/* is this a UDP packet? */
	{ BPF_JMP+BPF_JEQ+BPF_K, 0, 5, IPPROTO_UDP },
	{ BPF_LDX+BPF_B+BPF_MSH, 0, 0, 0 },
	{ BPF_LD+BPF_H+BPF_IND, 0, 0, 0 },	/* source port 12345? */
	{ BPF_JMP+BPF_JEQ+BPF_K, 0, 2, TEST_PORT_A },
	{ BPF_LD+BPF_H+BPF_IND, 0, 0, 2 },	/* destination port 12346? */
	{ BPF_JMP+BPF_JEQ+BPF_K, 1, 0, TEST_PORT_B },
	{ BPF_RET+BPF_K, 0, 0, 0 },		/* reject the packet */
	{ BPF_RET+BPF_K, 0, 0, (uint32_t)-1 },	/* accept the (whole) packet */
};

/*
 * Set up a BPF device, a pair of sockets of which traffic will be captured on
 * the BPF device, a buffer for capturing packets, and optionally a filter.
 * If the given size is non-zero, use that as buffer size.  Return the BPF
 * device's actual buffer size, which is also the size of 'buf'.
 */
static size_t
test94_setup(int * fd, int * fd2, int * fd3, uint8_t ** buf, unsigned int size,
	int set_filter)
{
	struct sockaddr_in sinA, sinB;
	struct ifreq ifr;
	struct bpf_program bf;
	unsigned int dlt;

	if ((*fd = open(_PATH_BPF, O_RDWR)) < 0) e(0);

	if (size != 0 && ioctl(*fd, BIOCSBLEN, &size) != 0) e(0);

	if (ioctl(*fd, BIOCGBLEN, &size) != 0) e(0);
	if (size < 1024 || size > BPF_MAXBUFSIZE) e(0);

	if ((*buf = malloc(size)) == NULL) e(0);

	if (set_filter) {
		/*
		 * Install a filter to improve predictability for the tests.
		 */
		memset(&bf, 0, sizeof(bf));
		bf.bf_len = __arraycount(test94_filter);
		bf.bf_insns = test94_filter;
		if (ioctl(*fd, BIOCSETF, &bf) != 0) e(0);
	}

	/* Bind to the loopback device. */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, LOOPBACK_IFNAME, sizeof(ifr.ifr_name));
	if (ioctl(*fd, BIOCSETIF, &ifr) != 0) e(0);

	/*
	 * If the loopback device's data link type is not DLT_RAW, our filter
	 * and size calculations will not work.
	 */
	if (ioctl(*fd, BIOCGDLT, &dlt) != 0) e(0);
	if (dlt != DLT_RAW) e(0);

	/* We use UDP traffic for our test packets. */
	if ((*fd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sinA, 0, sizeof(sinA));
	sinA.sin_family = AF_INET;
	sinA.sin_port = htons(TEST_PORT_A);
	sinA.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(*fd2, (struct sockaddr *)&sinA, sizeof(sinA)) != 0) e(0);

	memcpy(&sinB, &sinA, sizeof(sinB));
	sinB.sin_port = htons(TEST_PORT_B);
	if (connect(*fd2, (struct sockaddr *)&sinB, sizeof(sinB)) != 0) e(0);

	if ((*fd3 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	if (bind(*fd3, (struct sockaddr *)&sinB, sizeof(sinB)) != 0) e(0);

	if (connect(*fd3, (struct sockaddr *)&sinA, sizeof(sinA)) != 0) e(0);

	return size;
}

/*
 * Clean up resources allocated by test94_setup().
 */
static void
test94_cleanup(int fd, int fd2, int fd3, uint8_t * buf)
{

	if (close(fd3) != 0) e(0);

	if (close(fd2) != 0) e(0);

	free(buf);

	if (close(fd) != 0) e(0);
}

/*
 * Test reading packets from a BPF device, using regular mode.
 */
static void
test94a(void)
{
	struct bpf_program bf;
	struct timeval tv;
	fd_set fds;
	uint8_t *buf;
	pid_t pid;
	size_t size;
	ssize_t len;
	uint32_t seq;
	int fd, fd2, fd3, status, bytes, fl;

	subtest = 1;

	size = test94_setup(&fd, &fd2, &fd3, &buf, 0 /*size*/,
	    0 /*set_filter*/);

	/*
	 * Test that a filled-up store buffer will be returned to a pending
	 * read call.  Perform this first test without a filter, to ensure that
	 * the default behavior is to accept all packets.  The side effect is
	 * that we may receive other loopback traffic as part of our capture.
	 */
	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		usleep(SLEEP_TIME);

		test94_fill_random(fd2, buf, size);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	len = read(fd, buf, size);

	if (len < size * 3/4) e(0);
	if (len > size) e(0);
	test94_check(buf, len, 1 /*seq*/, 0 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/* Only the exact buffer size may be used in read calls. */
	if (read(fd, buf, size - 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (read(fd, buf, size + 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (read(fd, buf, sizeof(struct bpf_hdr)) != -1) e(0);
	if (errno != EINVAL) e(0);

	/*
	 * Install a filter to improve predictability for the remaining tests.
	 */
	memset(&bf, 0, sizeof(bf));
	bf.bf_len = __arraycount(test94_filter);
	bf.bf_insns = test94_filter;
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	/*
	 * Next we want to test that an already filled-up buffer will be
	 * returned to a read call immediately.  We take the opportunity to
	 * test that filling the buffer will also wake up a blocked select
	 * call.  In addition, we test ioctl(FIONREAD).
	 */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);
	if (FD_ISSET(fd, &fds)) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);
	if (bytes != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		usleep(SLEEP_TIME);

		test94_fill_random(fd2, buf, size);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);

	if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	len = read(fd, buf, size);

	if (len < size * 3/4) e(0);
	if (len > size) e(0);
	seq = test94_check(buf, len, 1 /*seq*/, 1 /*filtered*/,
	    NULL /*caplen*/, NULL /*datalen*/);

	if (len != bytes) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/* There is one more packet in the store buffer at this point. */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);
	if (FD_ISSET(fd, &fds)) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);
	if (bytes != 0) e(0);

	/*
	 * Next, we test whether read timeouts work, first checking that a
	 * timed-out read call returns any packets currently in the buffer.
	 * We use sleep and a signal as a crude way to test that the call was
	 * actually blocked until the timeout occurred.
	 */
	got_signal = 0;

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		signal(SIGUSR1, test94_signal);

		usleep(SLEEP_TIME);

		test94_add_random(fd2, buf, size, seq + 1);

		usleep(SLEEP_TIME);

		if (got_signal != 0) e(0);
		pause();
		if (got_signal != 1) e(0);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	tv.tv_sec = 0;
	tv.tv_usec = SLEEP_TIME * 3;
	if (ioctl(fd, BIOCSRTIMEOUT, &tv) != 0) e(0);

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* two packets < 3/4 of the size */
	if (test94_check(buf, len, seq, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq + 2) e(0);

	if (kill(pid, SIGUSR1) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * Next, see if a timed-out read will all buffers empty yields EAGAIN.
	 */
	tv.tv_sec = 0;
	tv.tv_usec = SLEEP_TIME;
	if (ioctl(fd, BIOCSRTIMEOUT, &tv) != 0) e(0);

	if (read(fd, buf, size) != -1) e(0);
	if (errno != EAGAIN) e(0);

	/*
	 * Verify that resetting the timeout to zero makes the call block
	 * forever (for short test values of "forever" anyway), because
	 * otherwise this may create a false illusion of correctness in the
	 * next test, for non-blocking calls.  As a side effect, this tests
	 * read call signal interruption, and ensures no partial results are
	 * returned in that case.
	 */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (ioctl(fd, BIOCSRTIMEOUT, &tv) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		signal(SIGUSR1, test94_signal);

		if (read(fd, buf, size) != -1) e(0);
		if (errno != EINTR) e(0);

		if (got_signal != 1) e(0);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	usleep(SLEEP_TIME * 2);

	if (kill(pid, SIGUSR1) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * Repeat the same test with a non-full, non-empty buffer, to ensure
	 * that interrupted reads do not return partial results.
	 */
	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		signal(SIGUSR1, test94_signal);

		if (read(fd, buf, size) != -1) e(0);
		if (errno != EINTR) e(0);

		if (got_signal != 1) e(0);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	usleep(SLEEP_TIME);

	test94_add_random(fd2, buf, size, 2);

	usleep(SLEEP_TIME);

	if (kill(pid, SIGUSR1) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * Test non-blocking reads with empty, full, and non-empty buffers.
	 * Against common sense, the last case should return whatever is in
	 * the buffer rather than EAGAIN, like immediate-mode reads would.
	 */
	if ((fl = fcntl(fd, F_GETFL)) == -1) e(0);
	if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) e(0);

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* one packet < 3/4 of the size */
	seq = test94_check(buf, len, 2 /*seq*/, 1 /*filtered*/,
	    NULL /*caplen*/, NULL /*datalen*/);

	if (read(fd, buf, size) != -1) e(0);
	if (errno != EAGAIN) e(0);

	test94_fill_random(fd2, buf, size);

	len = read(fd, buf, size);
	if (len < size * 3/4) e(0);
	if (len > size) e(0);
	seq = test94_check(buf, len, 1 /*seq*/, 1 /*filtered*/,
	    NULL /*caplen*/, NULL /*datalen*/);

	len = read(fd, buf, size);

	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* one packet < 3/4 of the size */
	if (test94_check(buf, len, seq, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq + 1) e(0);

	if (fcntl(fd, F_SETFL, fl) != 0) e(0);

	/*
	 * Test two remaining aspects of select(2): single-packet arrivals do
	 * not cause a wake-up, and the read timer has no effect.  The latter
	 * is a deliberate implementation choice where we diverge from NetBSD,
	 * because it requires keeping state in a way that violates the
	 * principle of system call independence.
	 */
	tv.tv_sec = 0;
	tv.tv_usec = SLEEP_TIME * 2;
	if (ioctl(fd, BIOCSRTIMEOUT, &tv) != 0) e(0);

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		usleep(SLEEP_TIME);

		test94_add_random(fd2, buf, size, 1);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	test94_cleanup(fd, fd2, fd3, buf);
}

/*
 * Test reading packets from a BPF device, using immediate mode.
 */
static void
test94b(void)
{
	struct timeval tv;
	fd_set fds;
	uint8_t *buf;
	unsigned int val;
	size_t size;
	ssize_t len;
	uint32_t seq;
	pid_t pid;
	int fd, fd2, fd3, bytes, status, fl;

	subtest = 2;

	size = test94_setup(&fd, &fd2, &fd3, &buf, 0 /*size*/,
	    1 /*set_filter*/);

	val = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &val) != 0) e(0);

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);
	if (bytes != 0) e(0);

	/*
	 * Ensure that if the hold buffer is full, an immediate-mode read
	 * returns the content of the hold buffer, even if the store buffer is
	 * not empty.
	 */
	test94_fill_random(fd2, buf, size);

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);

	len = read(fd, buf, size);
	if (len < size * 3/4) e(0);
	if (len > size) e(0);
	seq = test94_check(buf, len, 1 /*seq*/, 1 /*filtered*/,
	    NULL /*caplen*/, NULL /*datalen*/);

	if (len != bytes) e(0);

	/*
	 * There is one packet left in the buffer.  In immediate mode, this
	 * packet should be returned immediately.
	 */
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* one packet < 3/4 of the size */
	if (test94_check(buf, len, seq, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq + 1) e(0);

	if (len != bytes) e(0);

	/* The buffer is now empty again. */
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 0) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);
	if (bytes != 0) e(0);

	/*
	 * Immediate-mode reads may return multiple packets from the store
	 * buffer.
	 */
	test94_add_random(fd2, buf, size, seq + 1);
	test94_add_random(fd2, buf, size, seq + 2);

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, &tv) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* two packets < 3/4 of the size */
	if (test94_check(buf, len, seq + 1, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq + 3) e(0);

	if (len != bytes) e(0);

	/*
	 * Now test waking up suspended calls, read(2) first.
	 */
	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		usleep(SLEEP_TIME);

		test94_add_random(fd2, buf, size, seq + 3);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* one packet < 3/4 of the size */
	if (test94_check(buf, len, seq + 3, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq + 4) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * Then select(2).
	 */
	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		usleep(SLEEP_TIME);

		test94_add_random(fd2, buf, size, seq + 4);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);

	if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* one packet < 3/4 of the size */
	if (test94_check(buf, len, seq + 4, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq + 5) e(0);

	if (len != bytes) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * Non-blocking reads should behave just as with regular mode.
	 */
	if ((fl = fcntl(fd, F_GETFL)) == -1) e(0);
	if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) e(0);

	if (read(fd, buf, size) != -1) e(0);
	if (errno != EAGAIN) e(0);

	test94_fill_random(fd2, buf, size);

	len = read(fd, buf, size);
	if (len < size * 3/4) e(0);
	if (len > size) e(0);
	seq = test94_check(buf, len, 1 /*seq*/, 1 /*filtered*/,
	    NULL /*caplen*/, NULL /*datalen*/);

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* one packet < 3/4 of the size */
	if (test94_check(buf, len, seq, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq + 1) e(0);

	if (fcntl(fd, F_SETFL, fl) != 0) e(0);

	/*
	 * Timeouts should work with immediate mode.
	 */
	tv.tv_sec = 0;
	tv.tv_usec = SLEEP_TIME;
	if (ioctl(fd, BIOCSRTIMEOUT, &tv) != 0) e(0);

	if (read(fd, buf, size) != -1) e(0);
	if (errno != EAGAIN) e(0);

	test94_cleanup(fd, fd2, fd3, buf);
}

/*
 * Test reading packets from a BPF device, with an exactly filled buffer.  The
 * idea is that normally the store buffer is considered "full" if the next
 * packet does not fit in it, but if no more bytes are left in it, it can be
 * rotated immediately.  This is a practically useless edge case, but we
 * support it, so we might as well test it.  Also, some of the code for this
 * case is shared with other rare cases that we cannot test here (interfaces
 * disappearing, to be specific), and exactly filling up the buffers does test
 * some other bounds checks so all that might make this worth it anyway.  While
 * we are exercising full control over our buffers, also check statistics.
 */
static void
test94c(void)
{
	struct bpf_stat bs;
	fd_set fds;
	uint8_t *buf;
	size_t size;
	pid_t pid;
	uint32_t count, seq;
	int fd, fd2, fd3, bytes, status, fl;

	subtest = 3;

	size = test94_setup(&fd, &fd2, &fd3, &buf, 0 /*size*/,
	    1 /*set_filter*/);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_capt != 0) e(0);
	if (bs.bs_drop != 0) e(0);

	/*
	 * Test read, select, and ioctl(FIONREAD) on an exactly filled buffer.
	 */
	count = test94_fill_exact(fd2, buf, size, 0);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_capt != count) e(0);
	if (bs.bs_recv < bs.bs_capt) e(0); /* may be more */
	if (bs.bs_drop != 0) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);
	if (bytes != size) e(0);

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if (read(fd, buf, size) != size) e(0);
	test94_check(buf, size, 0 /*seq*/, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/);

	/*
	 * If the store buffer is full, the buffers should be swapped after
	 * emptying the hold buffer.
	 */
	seq = test94_fill_exact(fd2, buf, size, 1);
	test94_fill_exact(fd2, buf, size, seq);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_capt != count * 3) e(0);
	if (bs.bs_recv < bs.bs_capt) e(0); /* may be more */
	if (bs.bs_drop != 0) e(0);

	test94_add_random(fd2, buf, size, 0); /* this one will get dropped */

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_capt != count * 3 + 1) e(0);
	if (bs.bs_recv < bs.bs_capt) e(0); /* may be more */
	if (bs.bs_drop != 1) e(0);

	test94_add_random(fd2, buf, size, 0); /* this one will get dropped */

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_capt != count * 3 + 2) e(0);
	if (bs.bs_recv < bs.bs_capt) e(0); /* may be more */
	if (bs.bs_drop != 2) e(0);

	if (ioctl(fd, FIONREAD, &bytes) != 0) e(0);
	if (bytes != size) e(0);

	if (read(fd, buf, size) != size) e(0);
	if (test94_check(buf, size, 1 /*seq*/, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != seq) e(0);

	if (read(fd, buf, size) != size) e(0);
	if (test94_check(buf, size, seq, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != count * 2 + 1) e(0);

	/*
	 * See if an exactly filled buffer resumes reads...
	 */
	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		usleep(SLEEP_TIME);

		test94_fill_exact(fd2, buf, size, 1);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	if (read(fd, buf, size) != size) e(0);
	test94_check(buf, size, 1 /*seq*/, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/*
	 * ...and selects.
	 */
	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		usleep(SLEEP_TIME);

		test94_fill_exact(fd2, buf, size, seq);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, &fds, NULL, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	if ((fl = fcntl(fd, F_GETFL)) == -1) e(0);
	if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) e(0);

	if (read(fd, buf, size) != size) e(0);
	test94_check(buf, size, seq, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/);

	if (read(fd, buf, size) != -1) e(0);
	if (errno != EAGAIN) e(0);

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_capt != count * 5 + 2) e(0);
	if (bs.bs_recv < bs.bs_capt) e(0); /* may be more */
	if (bs.bs_drop != 2) e(0);

	test94_cleanup(fd, fd2, fd3, buf);
}

/*
 * Test receipt of large packets on BPF devices.  Large packets should be
 * truncated to the size of the buffer, but unless the filter specifies a
 * smaller capture size, no more than that.
 */
static void
test94d(void)
{
	struct bpf_hdr bh;
	uint8_t *buf, *buf2;
	size_t size;
	ssize_t len;
	int fd, fd2, fd3, datalen;

	subtest = 4;

	/*
	 * Specify a size smaller than the largest packet we can send on the
	 * loopback device.  The size we specify here is currently the default
	 * size already anyway, but that might change in the future.
	 */
	size = test94_setup(&fd, &fd2, &fd3, &buf, 32768 /*size*/,
	    1 /*set_filter*/);
	if (size != 32768) e(0);

	datalen = 65000;
	if (setsockopt(fd2, SOL_SOCKET, SO_SNDBUF, &datalen,
	    sizeof(datalen)) != 0) e(0);

	if ((buf2 = malloc(datalen)) == NULL) e(0);

	memset(buf2, 'Y', datalen);
	buf2[0] = 'X';
	buf2[size - sizeof(struct udphdr) - sizeof(struct ip) -
	    BPF_WORDALIGN(sizeof(bh)) - 1] = 'Z';

	if (write(fd2, buf2, datalen) != datalen) e(0);

	if (read(fd, buf, size) != size) e(0);

	memcpy(&bh, buf, sizeof(bh));

	if (bh.bh_hdrlen != BPF_WORDALIGN(sizeof(bh))) e(0);
	if (bh.bh_caplen != size - BPF_WORDALIGN(sizeof(bh))) e(0);
	if (bh.bh_datalen !=
	    sizeof(struct ip) + sizeof(struct udphdr) + datalen) e(0);

	if (buf[BPF_WORDALIGN(sizeof(bh)) + sizeof(struct ip) +
	    sizeof(struct udphdr)] != 'X') e(0);
	if (buf[size - 2] != 'Y') e(0);
	if (buf[size - 1] != 'Z') e(0);

	/*
	 * Add a smaller packet in between, to ensure that 1) the large packet
	 * is not split across buffers, and 2) the packet is truncated to the
	 * size of the buffer, not the available part of the buffer.  Note how
	 * forced rotation and our exact-fill policy preclude us from having to
	 * use immediate mode for any of this.
	 */
	test94_add_random(fd2, buf, size, 1 /*seq*/);

	if (write(fd2, buf2, datalen) != datalen) e(0);

	len = read(fd, buf, size);
	if (len <= 0) e(0);
	if (len >= size * 3/4) e(0);	/* one packet < 3/4 of the size */
	if (test94_check(buf, len, 1 /*seq*/, 1 /*filtered*/, NULL /*caplen*/,
	    NULL /*datalen*/) != 2) e(0);

	if (read(fd, buf, size) != size) e(0);

	memcpy(&bh, buf, sizeof(bh));

	if (bh.bh_hdrlen != BPF_WORDALIGN(sizeof(bh))) e(0);
	if (bh.bh_caplen != size - BPF_WORDALIGN(sizeof(bh))) e(0);
	if (bh.bh_datalen !=
	    sizeof(struct ip) + sizeof(struct udphdr) + datalen) e(0);

	if (buf[BPF_WORDALIGN(sizeof(bh)) + sizeof(struct ip) +
	    sizeof(struct udphdr)] != 'X') e(0);
	if (buf[size - 2] != 'Y') e(0);
	if (buf[size - 1] != 'Z') e(0);

	free(buf2);

	test94_cleanup(fd, fd2, fd3, buf);
}

/*
 * Test whether our filter is active through two-way communication and a
 * subsequent check on the BPF statistics.  We do not actually look through the
 * captured packets, because who knows what else is active on the loopback
 * device (e.g., X11) and the extra code specifically to extract our packets in
 * the other direction is simply not worth it.
 */
static void
test94_comm(int fd, int fd2, int fd3, int filtered)
{
	struct bpf_stat bs;
	char c;

	if (write(fd2, "A", 1) != 1) e(0);

	if (read(fd3, &c, 1) != 1) e(0);
	if (c != 'A') e(0);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv == 0) e(0);
	if (bs.bs_capt == 0) e(0);

	if (ioctl(fd, BIOCFLUSH) != 0) e(0);

	if (write(fd3, "B", 1) != 1) e(0);

	if (read(fd2, &c, 1) != 1) e(0);
	if (c != 'B') e(0);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv == 0) e(0);

	if (filtered) {
		if (bs.bs_capt != 0) e(0);
		if (bs.bs_drop != 0) e(0);
	} else
		if (bs.bs_capt == 0) e(0);

	if (ioctl(fd, BIOCFLUSH) != 0) e(0);
}

/*
 * Test filter installation and mechanics.
 */
static void
test94e(void)
{
	struct bpf_program bf;
	struct bpf_stat bs;
	struct bpf_hdr bh;
	uint8_t *buf;
	size_t size, len, plen, alen, off;
	uint32_t seq, caplen[4], datalen[4];
	int i, fd, fd2, fd3, val;

	subtest = 5;

	/*
	 * We have already tested installing a filter both before and after
	 * attaching to an interface by now, so we do not repeat that here.
	 */
	size = test94_setup(&fd, &fd2, &fd3, &buf, 0 /*size*/,
	    0 /*set_filter*/);

	val = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &val) != 0) e(0);

	/*
	 * A filter that is too large is rejected.  Unfortunately, due to
	 * necessary IOCTL rewriting, this tests libc, not the service.
	 */
	memset(&bf, 0, sizeof(bf));
	bf.bf_len = BPF_MAXINSNS + 1;
	bf.bf_insns = NULL;
	if (ioctl(fd, BIOCSETF, &bf) != -1) e(0);
	if (errno != EINVAL) e(0);

	/*
	 * An invalid filter is rejected.  In this test case, the truncated
	 * filter has a jump target beyond the end of the filter program.
	 */
	memset(&bf, 0, sizeof(bf));
	bf.bf_len = __arraycount(test94_filter) - 1;
	bf.bf_insns = test94_filter;
	if (ioctl(fd, BIOCSETF, &bf) != -1) e(0);
	if (errno != EINVAL) e(0);

	test94_comm(fd, fd2, fd3, 0 /*filtered*/);

	bf.bf_len++;
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	test94_comm(fd, fd2, fd3, 1 /*filtered*/);

	/*
	 * Installing a zero-length filter clears the current filter, if any.
	 */
	memset(&bf, 0, sizeof(bf));
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	test94_comm(fd, fd2, fd3, 0 /*filtered*/);

	/* Test this twice to trip over unconditional filter deallocation. */
	memset(&bf, 0, sizeof(bf));
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	test94_comm(fd, fd2, fd3, 0 /*filtered*/);

	/*
	 * Test both aligned and unaligned capture sizes.  For each, test
	 * sizes larger than, equal to, and smaller than the capture size.
	 * In both cases, aggregate the packets into a single buffer and only
	 * then go through them, to see whether alignment was done correctly.
	 * We cannot do everything in one go as BIOCSETF implies a BIOCFLUSH.
	 */
	plen = sizeof(struct ip) + sizeof(struct udphdr) + sizeof(seq);
	if (BPF_WORDALIGN(plen) != plen) e(0);
	alen = BPF_WORDALIGN(plen + 1);
	if (alen - 2 <= plen + 1) e(0);

	/* First the aligned cases. */
	test94_filter[__arraycount(test94_filter) - 1].k = alen;

	memset(&bf, 0, sizeof(bf));
	bf.bf_len = __arraycount(test94_filter);
	bf.bf_insns = test94_filter;
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	test94_comm(fd, fd2, fd3, 1 /*filtered*/);

	test94_add_specific(fd2, buf, alen + 1 - plen, 1);
	caplen[0] = alen;
	datalen[0] = alen + 1;

	test94_add_specific(fd2, buf, alen - plen, 2);
	caplen[1] = alen;
	datalen[1] = alen;

	test94_add_specific(fd2, buf, alen + 3 - plen, 3);
	caplen[2] = alen;
	datalen[2] = alen + 3;

	test94_add_specific(fd2, buf, alen - 1 - plen, 4);
	caplen[3] = alen - 1;
	datalen[3] = alen - 1;

	memset(buf, 0, size);

	len = read(fd, buf, size);

	if (test94_check(buf, len, 1 /*seq*/, 1 /*filtered*/, caplen,
	    datalen) != 5) e(0);

	/* Then the unaligned cases. */
	test94_filter[__arraycount(test94_filter) - 1].k = alen + 1;
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	test94_add_specific(fd2, buf, alen + 2 - plen, 5);
	caplen[0] = alen + 1;
	datalen[0] = alen + 2;

	test94_add_specific(fd2, buf, alen + 1 - plen, 6);
	caplen[1] = alen + 1;
	datalen[1] = alen + 1;

	test94_add_specific(fd2, buf, alen + 9 - plen, 7);
	caplen[2] = alen + 1;
	datalen[2] = alen + 9;

	test94_add_specific(fd2, buf, alen - plen, 8);
	caplen[3] = alen;
	datalen[3] = alen;

	memset(buf, 0, size);

	len = read(fd, buf, size);

	if (test94_check(buf, len, 5 /*seq*/, 1 /*filtered*/, caplen,
	    datalen) != 9) e(0);

	/*
	 * Check that capturing only one byte from packets is possible.  Not
	 * that that would be particularly useful.
	 */
	test94_filter[__arraycount(test94_filter) - 1].k = 1;
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	test94_add_random(fd2, buf, size, 9);
	test94_add_random(fd2, buf, size, 10);
	test94_add_random(fd2, buf, size, 11);

	memset(buf, 0, size);

	len = read(fd, buf, size);
	if (len <= 0) e(0);

	off = 0;
	for (i = 0; i < 3; i++) {
		if (len - off < sizeof(bh)) e(0);
		memcpy(&bh, &buf[off], sizeof(bh));

		if (bh.bh_tstamp.tv_sec == 0 && bh.bh_tstamp.tv_usec == 0)
			e(0);
		if (bh.bh_caplen != 1) e(0);
		if (bh.bh_datalen < plen) e(0);
		if (bh.bh_hdrlen != BPF_WORDALIGN(sizeof(bh))) e(0);

		off += bh.bh_hdrlen;

		if (buf[off] != 0x45) e(0);

		off += BPF_WORDALIGN(bh.bh_caplen);
	}
	if (off != len) e(0);

	/*
	 * Finally, a zero capture size should result in rejected packets only.
	 */
	test94_filter[__arraycount(test94_filter) - 1].k = 0;
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	test94_add_random(fd2, buf, size, 12);
	test94_add_random(fd2, buf, size, 13);
	test94_add_random(fd2, buf, size, 14);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv < 3) e(0);
	if (bs.bs_capt != 0) e(0);
	if (bs.bs_drop != 0) e(0);

	/* Restore the capture limit of the filter to its original state. */
	test94_filter[__arraycount(test94_filter) - 1].k = (uint32_t)-1;

	test94_cleanup(fd, fd2, fd3, buf);
}

/*
 * Compute an IP checksum.
 */
static uint16_t
test94_cksum(uint8_t * buf, size_t len)
{
	uint32_t sum, word;

	/* This is a really dumb implementation but *shrug*. */
	for (sum = 0; len > 0; sum += word) {
		if (len > 1) {
			word = buf[0] << 8 | buf[1];
			buf += 2;
			len -= 2;
		} else {
			word = buf[0] << 8;
			len--;
		}
	}

	while (sum > UINT16_MAX)
		sum = (sum & UINT16_MAX) + (sum >> 16);

	return ~(uint16_t)sum;
}

/*
 * Set up UDP headers for a packet.  The packet uses IPv4 unless 'v6' is set,
 * in which case IPv6 is used.  The given buffer must be large enough to
 * contain the headers and the (to be appended) data.  The function returns the
 * offset into the buffer to the data portion of the packet.
 */
static size_t
test94_make_pkt(uint8_t * buf, size_t len, int v6)
{
	struct ip ip;
	struct ip6_hdr ip6;
	struct udphdr uh;
	size_t off;

	if (!v6) {
		memset(&ip, 0, sizeof(ip));
		ip.ip_v = IPVERSION;
		ip.ip_hl = sizeof(ip) >> 2;
		ip.ip_len = htons(sizeof(ip) + sizeof(uh) + len);
		ip.ip_ttl = 255;
		ip.ip_p = IPPROTO_UDP;
		ip.ip_sum = 0;
		ip.ip_src.s_addr = htonl(INADDR_LOOPBACK);
		ip.ip_dst.s_addr = htonl(INADDR_LOOPBACK);

		memcpy(buf, &ip, sizeof(ip));
		ip.ip_sum = htons(test94_cksum(buf, sizeof(ip)));
		memcpy(buf, &ip, sizeof(ip));
		if (test94_cksum(buf, sizeof(ip)) != 0) e(0);

		off = sizeof(ip);
	} else {
		memset(&ip6, 0, sizeof(ip6));
		ip6.ip6_vfc = IPV6_VERSION;
		ip6.ip6_plen = htons(sizeof(uh) + len);
		ip6.ip6_nxt = IPPROTO_UDP;
		ip6.ip6_hlim = 255;
		memcpy(&ip6.ip6_src, &in6addr_loopback, sizeof(ip6.ip6_src));
		memcpy(&ip6.ip6_dst, &in6addr_loopback, sizeof(ip6.ip6_dst));

		memcpy(buf, &ip6, sizeof(ip6));

		off = sizeof(ip6);
	}

	memset(&uh, 0, sizeof(uh));
	uh.uh_sport = htons(TEST_PORT_A);
	uh.uh_dport = htons(TEST_PORT_B);
	uh.uh_ulen = htons(sizeof(uh) + len);
	uh.uh_sum = 0; /* lazy but we also don't have the data yet */

	memcpy(buf + off, &uh, sizeof(uh));

	return off + sizeof(uh);
}

/*
 * Test sending packets by writing to a BPF device.
 */
static void
test94f(void)
{
	struct bpf_stat bs;
	struct ifreq ifr;
	fd_set fds;
	uint8_t *buf;
	size_t off;
	unsigned int i, uval, mtu;
	int fd, fd2, fd3;

	subtest = 6;

	(void)test94_setup(&fd, &fd2, &fd3, &buf, 0 /*size*/,
	    1 /*set_filter*/);

	/*
	 * Select queries should always indicate that the device is writable.
	 */
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, NULL, &fds, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	/*
	 * Test packet size limits.  For loopback devices, the maximum data
	 * link layer level maximum transmission unit should be 65535-4 =
	 * 65531 bytes.  Obtain the actual value anyway; it might have changed.
	 */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, LOOPBACK_IFNAME, sizeof(ifr.ifr_name));

	if (ioctl(fd2, SIOCGIFMTU, &ifr) != 0) e(0);
	mtu = ifr.ifr_mtu;

	if ((buf = realloc(buf, UINT16_MAX + 1)) == NULL) e(0);

	memset(buf, 0, UINT16_MAX + 1);

	for (i = UINT16_MAX + 1; i > mtu; i--) {
		if (write(fd, buf, i) != -1) e(0);
		if (errno != EMSGSIZE) e(0);
	}

	/* This packet will be discarded as completely crap.  That's fine. */
	if (write(fd, buf, mtu) != mtu) e(0);

	/*
	 * Zero-sized writes are accepted but do not do anything.
	 */
	if (write(fd, buf, 0) != 0) e(0);

	/*
	 * Send an actual packet, and see if it arrives.
	 */
	off = test94_make_pkt(buf, 6, 0 /*v6*/);
	memcpy(buf + off, "Hello!", 6);

	if (write(fd, buf, off + 6) != off + 6) e(0);

	memset(buf, 0, mtu);
	if (read(fd3, buf, mtu) != 6) e(0);
	if (memcmp(buf, "Hello!", 6) != 0) e(0);

	/*
	 * Enable feedback mode to test that the packet now arrives twice.
	 * Send a somewhat larger packet to test that data copy-in handles
	 * offsets correctly.
	 */
	uval = 1;
	if (ioctl(fd, BIOCSFEEDBACK, &uval) != 0) e(0);

	off = test94_make_pkt(buf, 12345, 0 /*v6*/);
	for (i = 0; i < 12345; i++)
		buf[off + i] = 1 + (i % 251); /* the largest prime < 255 */

	if (write(fd, buf, off + 12345) != off + 12345) e(0);

	/* We need a default UDP SO_RCVBUF >= 12345 * 2 for this. */
	memset(buf, 0, UINT16_MAX);
	if (recv(fd3, buf, UINT16_MAX, 0) != 12345) e(0);
	for (i = 0; i < 12345; i++)
		if (buf[i] != 1 + (i % 251)) e(0);

	memset(buf, 0, UINT16_MAX);
	if (recv(fd3, buf, UINT16_MAX, MSG_DONTWAIT) != 12345) e(0);
	for (i = 0; i < 12345; i++)
		if (buf[i] != 1 + (i % 251)) e(0);

	if (recv(fd3, buf, UINT16_MAX, MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	/*
	 * The two valid packets we sent will have been captured by our BPF
	 * device as well, because SEESENT is enabled by default and also
	 * applies to packets written to a BPF device.  The reason for that is
	 * that it allows tcpdump(8) to see what DHCP clients are sending, for
	 * example.  The packets we sent are accepted by the installed filter.
	 */
	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_capt != 2) e(0);

	/* Now that we've written data, test select once more. */
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	if (select(fd + 1, NULL, &fds, NULL, NULL) != 1) e(0);
	if (!FD_ISSET(fd, &fds)) e(0);

	test94_cleanup(fd, fd2, fd3, buf);
}

/*
 * Test read, write, and select operations on unconfigured devices.
 */
static void
test94g(void)
{
	fd_set rfds, wfds;
	uint8_t *buf;
	unsigned int size;
	int fd;

	subtest = 7;

	if ((fd = open(_PATH_BPF, O_RDWR)) < 0) e(0);

	if (ioctl(fd, BIOCGBLEN, &size) != 0) e(0);
	if (size < 1024 || size > BPF_MAXBUFSIZE) e(0);

	if ((buf = malloc(size)) == NULL) e(0);

	if (read(fd, buf, size) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (write(fd, buf, size) != -1) e(0);
	if (errno != EINVAL) e(0);

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);

	if (select(fd + 1, &rfds, &wfds, NULL, NULL) != 2) e(0);

	if (!FD_ISSET(fd, &rfds)) e(0);
	if (!FD_ISSET(fd, &wfds)) e(0);

	free(buf);

	if (close(fd) != 0) e(0);
}

/*
 * Test various IOCTL calls.  Several of these tests are rather superficial,
 * because we would need a real interface, rather than the loopback device, to
 * test their functionality properly.  Also note that we skip various checks
 * performed as part of the earlier subtests.
 */
static void
test94h(void)
{
	struct bpf_stat bs;
	struct bpf_version bv;
	struct bpf_dltlist bfl;
	struct ifreq ifr;
	struct timeval tv;
	uint8_t *buf;
	size_t size;
	unsigned int uval, list[2];
	int cfd, ufd, fd2, fd3, val;

	subtest = 8;

	/*
	 * Many IOCTLs work only on configured or only on unconfigured BPF
	 * devices, so for convenience we create a file descriptor for each.
	 */
	size = test94_setup(&cfd, &fd2, &fd3, &buf, 0 /*size*/,
	    1 /*set_filter*/);

	if ((ufd = open(_PATH_BPF, O_RDWR)) < 0) e(0);

	/*
	 * The BIOCSBLEN value is silently corrected to fall within a valid
	 * range, and BIOCGBLEN can be used to obtain the corrected value.  We
	 * do not know the valid range, so we use fairly extreme test values.
	 */
	uval = 1;
	if (ioctl(ufd, BIOCSBLEN, &uval) != 0) e(0);

	if (ioctl(ufd, BIOCGBLEN, &uval) != 0) e(0);
	if (uval < sizeof(struct bpf_hdr) || uval > BPF_MAXBUFSIZE) e(0);

	uval = (unsigned int)-1;
	if (ioctl(ufd, BIOCSBLEN, &uval) != 0) e(0);

	if (ioctl(ufd, BIOCGBLEN, &uval) != 0) e(0);
	if (uval < sizeof(struct bpf_hdr) || uval > BPF_MAXBUFSIZE) e(0);

	uval = 0;
	if (ioctl(ufd, BIOCSBLEN, &uval) != 0) e(0);

	if (ioctl(ufd, BIOCGBLEN, &uval) != 0) e(0);
	if (uval < sizeof(struct bpf_hdr) || uval > BPF_MAXBUFSIZE) e(0);

	uval = 1024; /* ..a value that should be acceptable but small */
	if (ioctl(ufd, BIOCSBLEN, &uval) != 0) e(0);
	if (ioctl(ufd, BIOCGBLEN, &uval) != 0) e(0);
	if (uval != 1024) e(0);

	/*
	 * For configured devices, it is not possible to adjust the buffer size
	 * but it is possible to obtain its size.
	 */
	if (ioctl(cfd, BIOCSBLEN, &uval) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (ioctl(cfd, BIOCGBLEN, &uval) != 0) e(0);
	if (uval != size) e(0);

	/*
	 * BIOCFLUSH resets both buffer contents and statistics.
	 */
	uval = 1;
	if (ioctl(cfd, BIOCIMMEDIATE, &uval) != 0) e(0);

	test94_fill_exact(fd2, buf, size, 1 /*seq*/);
	test94_fill_exact(fd2, buf, size, 1 /*seq*/);
	test94_fill_exact(fd2, buf, size, 1 /*seq*/);

	if (ioctl(cfd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv == 0) e(0);
	if (bs.bs_drop == 0) e(0);
	if (bs.bs_capt == 0) e(0);

	/* Do make sure that statistics are not cleared on retrieval.. */
	if (ioctl(cfd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv == 0) e(0);
	if (bs.bs_drop == 0) e(0);
	if (bs.bs_capt == 0) e(0);

	if (ioctl(cfd, FIONREAD, &val) != 0) e(0);
	if (val == 0) e(0);

	if (ioctl(cfd, BIOCFLUSH) != 0) e(0);

	/* There is a race condition for bs_recv here, so we cannot test it. */
	if (ioctl(cfd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_drop != 0) e(0);
	if (bs.bs_capt != 0) e(0);

	if (ioctl(cfd, FIONREAD, &val) != 0) e(0);
	if (val != 0) e(0);

	/*
	 * Although practically useless, BIOCFLUSH works on unconfigured
	 * devices.  So does BIOCGSTATS.
	 */
	if (ioctl(ufd, BIOCFLUSH) != 0) e(0);

	if (ioctl(ufd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv != 0) e(0);
	if (bs.bs_drop != 0) e(0);
	if (bs.bs_capt != 0) e(0);

	/*
	 * BIOCPROMISC works on configured devices only.  On loopback devices
	 * it has no observable effect though.
	 */
	if (ioctl(ufd, BIOCPROMISC) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (ioctl(cfd, BIOCPROMISC) != 0) e(0);

	/*
	 * BIOCGDLT does not work on unconfigured devices.
	 */
	if (ioctl(ufd, BIOCGDLT, &uval) != -1) e(0);
	if (errno != EINVAL) e(0);

	/*
	 * BIOCGETIF works only on configured devices, where it returns the
	 * associated device name.
	 */
	if (ioctl(ufd, BIOCGETIF, &ifr) != -1) e(0);
	if (errno != EINVAL) e(0);

	memset(&ifr, 'X', sizeof(ifr));
	if (ioctl(cfd, BIOCGETIF, &ifr) != 0) e(0);
	if (strcmp(ifr.ifr_name, LOOPBACK_IFNAME) != 0) e(0);

	/*
	 * BIOCSETIF works only on unconfigured devices, and accepts only valid
	 * valid interface names.  The name is forced to be null terminated.
	 */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, LOOPBACK_IFNAME, sizeof(ifr.ifr_name));
	if (ioctl(cfd, BIOCSETIF, &ifr) != -1) e(0);
	if (errno != EINVAL) e(0);

	memset(&ifr, 0, sizeof(ifr));
	memset(ifr.ifr_name, 'x', sizeof(ifr.ifr_name));
	if (ioctl(ufd, BIOCSETIF, &ifr) != -1) e(0);
	if (errno != ENXIO) e(0);

	/* Anyone that has ten loopback devices is simply insane. */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, LOOPBACK_IFNAME, sizeof(ifr.ifr_name));
	ifr.ifr_name[strlen(ifr.ifr_name) - 1] += 9;
	if (ioctl(ufd, BIOCSETIF, &ifr) != -1) e(0);
	if (errno != ENXIO) e(0);

	/*
	 * It is possible to turn BIOCIMMEDIATE on and off.  We already enabled
	 * it a bit higher up.  Note that our implementation does not support
	 * toggling the setting while a read call is no progress, and toggling
	 * the setting will have no effect while a select call is in progress;
	 * similar restrictions apply to effectively all relevant settings.
	 * Either way we do not test that here either.
	 */
	test94_add_random(fd2, buf, size, 1 /*seq*/);

	if (ioctl(cfd, FIONREAD, &val) != 0) e(0);
	if (val == 0) e(0);

	uval = 0;
	if (ioctl(cfd, BIOCIMMEDIATE, &uval) != 0) e(0);

	if (ioctl(cfd, FIONREAD, &val) != 0) e(0);
	if (val != 0) e(0);

	uval = 1;
	if (ioctl(cfd, BIOCIMMEDIATE, &uval) != 0) e(0);

	if (ioctl(cfd, FIONREAD, &val) != 0) e(0);
	if (val == 0) e(0);

	if (ioctl(cfd, BIOCFLUSH) != 0) e(0);

	/*
	 * BIOCIMMEDIATE also works on unconfigured devices.
	 */
	uval = 1;
	if (ioctl(ufd, BIOCIMMEDIATE, &uval) != 0) e(0);

	uval = 0;
	if (ioctl(ufd, BIOCIMMEDIATE, &uval) != 0) e(0);

	/*
	 * BIOCVERSION should return the current BPF interface version.
	 */
	if (ioctl(ufd, BIOCVERSION, &bv) != 0) e(0);
	if (bv.bv_major != BPF_MAJOR_VERSION) e(0);
	if (bv.bv_minor != BPF_MINOR_VERSION) e(0);

	/*
	 * BIOCSHDRCMPLT makes sense only for devices with data link headers,
	 * which rules out loopback devices.  Check the default and test
	 * toggling it, and stop there.
	 */
	/* The default value is off. */
	uval = 1;
	if (ioctl(ufd, BIOCGHDRCMPLT, &uval) != 0) e(0);
	if (uval != 0) e(0);

	uval = 2;
	if (ioctl(ufd, BIOCSHDRCMPLT, &uval) != 0) e(0);

	if (ioctl(ufd, BIOCGHDRCMPLT, &uval) != 0) e(0);
	if (uval != 1) e(0);

	uval = 0;
	if (ioctl(ufd, BIOCSHDRCMPLT, &uval) != 0) e(0);

	uval = 1;
	if (ioctl(ufd, BIOCGHDRCMPLT, &uval) != 0) e(0);
	if (uval != 0) e(0);

	/*
	 * BIOCSDLT works on configured devices.  For loopback devices, it can
	 * only set the data link type to its current value, which on MINIX3
	 * for loopback devices is DLT_RAW (i.e., no headers at all).
	 */
	uval = DLT_RAW;
	if (ioctl(ufd, BIOCSDLT, &uval) != -1) e(0);
	if (errno != EINVAL) e(0);

	uval = DLT_RAW;
	if (ioctl(cfd, BIOCSDLT, &uval) != 0) e(0);

	uval = DLT_NULL;
	if (ioctl(cfd, BIOCSDLT, &uval) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (ioctl(cfd, BIOCGDLT, &uval) != 0) e(0);
	if (uval != DLT_RAW) e(0);

	/*
	 * BIOCGDLTLIST works on configured devices only, and may be used to
	 * both query the size of the list and obtain the list.  On MINIX3,
	 * loopback devices will only ever return DLT_RAW.  Unfortunately,
	 * much of the handling for this IOCTL is in libc for us, which is also
	 * why we do not test bad pointers and stuff like that.
	 */
	memset(&bfl, 0, sizeof(bfl));
	if (ioctl(ufd, BIOCGDLTLIST, &bfl) != -1) e(0);
	if (errno != EINVAL) e(0);

	memset(&bfl, 0, sizeof(bfl));
	if (ioctl(cfd, BIOCGDLTLIST, &bfl) != 0) e(0);
	if (bfl.bfl_len != 1) e(0);
	if (bfl.bfl_list != NULL) e(0);

	memset(&bfl, 0, sizeof(bfl));
	bfl.bfl_len = 2;	/* should be ignored */
	if (ioctl(cfd, BIOCGDLTLIST, &bfl) != 0) e(0);
	if (bfl.bfl_len != 1) e(0);
	if (bfl.bfl_list != NULL) e(0);

	memset(&bfl, 0, sizeof(bfl));
	memset(list, 0, sizeof(list));
	bfl.bfl_list = list;
	if (ioctl(cfd, BIOCGDLTLIST, &bfl) != -1) e(0);
	if (errno != ENOMEM) e(0);
	if (list[0] != 0) e(0);

	memset(&bfl, 0, sizeof(bfl));
	bfl.bfl_len = 1;
	bfl.bfl_list = list;
	if (ioctl(cfd, BIOCGDLTLIST, &bfl) != 0) e(0);
	if (bfl.bfl_len != 1) e(0);
	if (bfl.bfl_list != list) e(0);
	if (list[0] != DLT_RAW) e(0);
	if (list[1] != 0) e(0);

	memset(&bfl, 0, sizeof(bfl));
	memset(list, 0, sizeof(list));
	bfl.bfl_len = 2;
	bfl.bfl_list = list;
	if (ioctl(cfd, BIOCGDLTLIST, &bfl) != 0) e(0);
	if (bfl.bfl_len != 1) e(0);
	if (bfl.bfl_list != list) e(0);
	if (list[0] != DLT_RAW) e(0);
	if (list[1] != 0) e(0);

	/*
	 * For loopback devices, BIOCSSEESENT is a bit weird: packets are
	 * captured on output to get a complete view of loopback traffic, and
	 * not also on input because that would then duplicate the traffic.  As
	 * a result, turning off BIOCSSEESENT for a loopback device means that
	 * no packets will be captured at all anymore.  First test the default
	 * and toggling on the unconfigured device, then reproduce the above on
	 * the configured device.
	 */
	/* The default value is on. */
	uval = 0;
	if (ioctl(ufd, BIOCGSEESENT, &uval) != 0) e(0);
	if (uval != 1) e(0);

	uval = 0;
	if (ioctl(ufd, BIOCSSEESENT, &uval) != 0) e(0);

	uval = 1;
	if (ioctl(ufd, BIOCGSEESENT, &uval) != 0) e(0);
	if (uval != 0) e(0);

	uval = 2;
	if (ioctl(ufd, BIOCSSEESENT, &uval) != 0) e(0);

	if (ioctl(ufd, BIOCGSEESENT, &uval) != 0) e(0);
	if (uval != 1) e(0);

	if (ioctl(cfd, BIOCGSEESENT, &uval) != 0) e(0);
	if (uval != 1) e(0);

	uval = 0;
	if (ioctl(cfd, BIOCSSEESENT, &uval) != 0) e(0);

	if (ioctl(cfd, BIOCFLUSH) != 0) e(0);

	test94_add_random(fd2, buf, size, 1 /*seq*/);

	if (ioctl(cfd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv != 0) e(0);

	uval = 1;
	if (ioctl(cfd, BIOCSSEESENT, &uval) != 0) e(0);

	if (ioctl(cfd, BIOCFLUSH) != 0) e(0);

	test94_add_random(fd2, buf, size, 1 /*seq*/);

	if (ioctl(cfd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv == 0) e(0);

	/*
	 * The BIOCSRTIMEOUT values are rounded up to clock granularity.
	 * Invalid timeout values are rejected.
	 */
	/* The default value is zero. */
	tv.tv_sec = 99;
	if (ioctl(ufd, BIOCGRTIMEOUT, &tv) != 0) e(0);
	if (tv.tv_sec != 0) e(0);
	if (tv.tv_usec != 0) e(0);

	tv.tv_usec = 1000000;
	if (ioctl(ufd, BIOCSRTIMEOUT, &tv) != -1) e(0);
	if (errno != EINVAL) e(0);

	tv.tv_usec = -1;
	if (ioctl(ufd, BIOCSRTIMEOUT, &tv) != -1) e(0);
	if (errno != EINVAL) e(0);

	tv.tv_sec = -1;
	tv.tv_usec = 0;
	if (ioctl(ufd, BIOCSRTIMEOUT, &tv) != -1) e(0);
	if (errno != EINVAL) e(0);

	tv.tv_sec = INT_MAX;
	if (ioctl(ufd, BIOCSRTIMEOUT, &tv) != -1) e(0);
	if (errno != EDOM) e(0);

	if (ioctl(ufd, BIOCGRTIMEOUT, &tv) != 0) e(0);
	if (tv.tv_sec != 0) e(0);
	if (tv.tv_usec != 0) e(0);

	tv.tv_sec = 123;
	tv.tv_usec = 1;
	if (ioctl(ufd, BIOCSRTIMEOUT, &tv) != 0) e(0);

	if (ioctl(ufd, BIOCGRTIMEOUT, &tv) != 0) e(0);
	if (tv.tv_sec != 123) e(0);
	if (tv.tv_usec == 0) e(0); /* rounding should be up */

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (ioctl(ufd, BIOCSRTIMEOUT, &tv) != 0) e(0);

	if (ioctl(ufd, BIOCGRTIMEOUT, &tv) != 0) e(0);
	if (tv.tv_sec != 0) e(0);
	if (tv.tv_usec != 0) e(0);

	/*
	 * BIOCSFEEDBACK is another weird setting for which we only test
	 * default and toggling here.
	 */
	/* The default value is off. */
	uval = 1;
	if (ioctl(ufd, BIOCGFEEDBACK, &uval) != 0) e(0);
	if (uval != 0) e(0);

	uval = 2;
	if (ioctl(ufd, BIOCSFEEDBACK, &uval) != 0) e(0);

	if (ioctl(ufd, BIOCGFEEDBACK, &uval) != 0) e(0);
	if (uval != 1) e(0);

	uval = 0;
	if (ioctl(ufd, BIOCSFEEDBACK, &uval) != 0) e(0);

	uval = 1;
	if (ioctl(ufd, BIOCGFEEDBACK, &uval) != 0) e(0);
	if (uval != 0) e(0);

	/* Clean up. */
	if (close(ufd) != 0) e(0);

	test94_cleanup(cfd, fd2, fd3, buf);
}

/* IPv6 version of our filter. */
static struct bpf_insn test94_filter6[] = {
	{ BPF_LD+BPF_B+BPF_ABS, 0, 0, 0 },	/* is this an IPv6 header? */
	{ BPF_ALU+BPF_RSH+BPF_K, 0, 0, 4 },
	{ BPF_JMP+BPF_JEQ+BPF_K, 0, 6, 6 },
	{ BPF_LD+BPF_B+BPF_ABS, 0, 0, 6 },	/* is this a UDP packet? */
	{ BPF_JMP+BPF_JEQ+BPF_K, 0, 4, IPPROTO_UDP },
	{ BPF_LD+BPF_H+BPF_ABS, 0, 0, 40 },	/* source port 12345? */
	{ BPF_JMP+BPF_JEQ+BPF_K, 0, 2, TEST_PORT_A },
	{ BPF_LD+BPF_H+BPF_ABS, 0, 0, 42 },	/* destination port 12346? */
	{ BPF_JMP+BPF_JEQ+BPF_K, 1, 0, TEST_PORT_B },
	{ BPF_RET+BPF_K, 0, 0, 0 },		/* reject the packet */
	{ BPF_RET+BPF_K, 0, 0, (uint32_t)-1 },	/* accept the (whole) packet */
};

/*
 * Test receipt of IPv6 packets, because it was getting a bit messy to
 * integrate that into the previous subtests.  We just want to make sure that
 * IPv6 packets are properly filtered and captured at all.  The rest of the
 * code is entirely version agnostic anyway.
 */
static void
test94i(void)
{
	struct sockaddr_in6 sin6A, sin6B;
	struct bpf_program bf;
	struct bpf_stat bs;
	struct bpf_hdr bh;
	struct ifreq ifr;
	struct ip6_hdr ip6;
	struct udphdr uh;
	uint8_t *buf, c;
	socklen_t socklen;
	ssize_t len;
	size_t off;
	unsigned int uval, size, dlt;
	int fd, fd2, fd3;

	subtest = 9;

	if ((fd = open(_PATH_BPF, O_RDWR)) < 0) e(0);

	if (ioctl(fd, BIOCGBLEN, &size) != 0) e(0);
	if (size < 1024 || size > BPF_MAXBUFSIZE) e(0);

	if ((buf = malloc(size)) == NULL) e(0);

	/* Install the filter. */
	memset(&bf, 0, sizeof(bf));
	bf.bf_len = __arraycount(test94_filter6);
	bf.bf_insns = test94_filter6;
	if (ioctl(fd, BIOCSETF, &bf) != 0) e(0);

	uval = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &uval) != 0) e(0);

	/* Bind to the loopback device. */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, LOOPBACK_IFNAME, sizeof(ifr.ifr_name));
	if (ioctl(fd, BIOCSETIF, &ifr) != 0) e(0);

	/*
	 * If the loopback device's data link type is not DLT_RAW, our filter
	 * and size calculations will not work.
	 */
	if (ioctl(fd, BIOCGDLT, &dlt) != 0) e(0);
	if (dlt != DLT_RAW) e(0);

	/* We use UDP traffic for our test packets. */
	if ((fd2 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin6A, 0, sizeof(sin6A));
	sin6A.sin6_family = AF_INET6;
	sin6A.sin6_port = htons(TEST_PORT_A);
	memcpy(&sin6A.sin6_addr, &in6addr_loopback, sizeof(sin6A.sin6_addr));
	if (bind(fd2, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	memcpy(&sin6B, &sin6A, sizeof(sin6B));
	sin6B.sin6_port = htons(TEST_PORT_B);
	if (connect(fd2, (struct sockaddr *)&sin6B, sizeof(sin6B)) != 0) e(0);

	if ((fd3 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	if (bind(fd3, (struct sockaddr *)&sin6B, sizeof(sin6B)) != 0) e(0);

	if (connect(fd3, (struct sockaddr *)&sin6A, sizeof(sin6A)) != 0) e(0);

	if (write(fd2, "A", 1) != 1) e(0);

	if (read(fd3, &c, 1) != 1) e(0);
	if (c != 'A') e(0);

	if (write(fd3, "B", 1) != 1) e(0);

	if (read(fd2, &c, 1) != 1) e(0);
	if (c != 'B') e(0);

	if (ioctl(fd, BIOCGSTATS, &bs) != 0) e(0);
	if (bs.bs_recv < 2) e(0);
	if (bs.bs_capt != 1) e(0);
	if (bs.bs_drop != 0) e(0);

	memset(buf, 0, size);

	len = read(fd, buf, size);

	if (len != BPF_WORDALIGN(sizeof(bh)) +
	    BPF_WORDALIGN(sizeof(ip6) + sizeof(uh) + 1)) e(0);

	memcpy(&bh, buf, sizeof(bh));

	if (bh.bh_tstamp.tv_sec == 0 && bh.bh_tstamp.tv_usec == 0) e(0);
	if (bh.bh_caplen != sizeof(ip6) + sizeof(uh) + 1) e(0);
	if (bh.bh_datalen != bh.bh_caplen) e(0);
	if (bh.bh_hdrlen != BPF_WORDALIGN(sizeof(bh))) e(0);

	if (buf[bh.bh_hdrlen + sizeof(ip6) + sizeof(uh)] != 'A') e(0);

	/*
	 * Finally, do a quick test to see if we can send IPv6 packets by
	 * writing to the BPF device.  We rely on such packets being generated
	 * properly in a later test.
	 */
	off = test94_make_pkt(buf, 6, 1 /*v6*/);
	memcpy(buf + off, "Hello!", 6);

	if (write(fd, buf, off + 6) != off + 6) e(0);

	socklen = sizeof(sin6A);
	if (recvfrom(fd3, buf, size, 0, (struct sockaddr *)&sin6A,
	    &socklen) != 6) e(0);

	if (memcmp(buf, "Hello!", 6) != 0) e(0);
	if (socklen != sizeof(sin6A)) e(0);
	if (sin6A.sin6_family != AF_INET6) e(0);
	if (sin6A.sin6_port != htons(TEST_PORT_A)) e(0);
	if (memcmp(&sin6A.sin6_addr, &in6addr_loopback,
	    sizeof(sin6A.sin6_addr)) != 0) e(0);

	free(buf);

	if (close(fd3) != 0) e(0);

	if (close(fd2) != 0) e(0);

	if (close(fd) != 0) e(0);
}

/*
 * Test the BPF sysctl(7) interface at a basic level.
 */
static void
test94j(void)
{
	struct bpf_stat bs1, bs2;
	struct bpf_d_ext *bde;
	uint8_t *buf;
	unsigned int slot, count, uval;
	size_t len, oldlen, size, bdesize;
	int fd, fd2, fd3, val, mib[5], smib[3], found;

	subtest = 10;

	/*
	 * Obtain the maximum buffer size.  The value must be sane.
	 */
	memset(mib, 0, sizeof(mib));
	len = __arraycount(mib);
	if (sysctlnametomib("net.bpf.maxbufsize", mib, &len) != 0) e(0);
	if (len != 3) e(0);

	oldlen = sizeof(val);
	if (sysctl(mib, len, &val, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(val)) e(0);

	if (val < 1024 || val > INT_MAX / 2) e(0);

	/*
	 * Attempt to set the maximum buffer size.  This is not (yet) supported
	 * so for now we want to make sure that it really does not work.
	 */
	if (sysctl(mib, len, NULL, NULL, &val, sizeof(val)) != -1) e(0);
	if (errno != EPERM) e(0);

	/*
	 * Obtain global statistics.  We check the actual statistics later on.
	 */
	memset(smib, 0, sizeof(smib));
	len = __arraycount(smib);
	if (sysctlnametomib("net.bpf.stats", smib, &len) != 0) e(0);
	if (len != 3) e(0);

	oldlen = sizeof(bs1);
	if (sysctl(smib, len, &bs1, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(bs1)) e(0);

	/*
	 * Set up a BPF descriptor, and retrieve the list of BPF peers.  We
	 * should be able to find our BPF peer.
	 */
	memset(mib, 0, sizeof(mib));
	len = __arraycount(mib);
	if (sysctlnametomib("net.bpf.peers", mib, &len) != 0) e(0);
	if (len != 3) e(0);
	mib[len++] = sizeof(*bde);	/* size of each element */
	mib[len++] = INT_MAX;		/* limit on elements to return */

	size = test94_setup(&fd, &fd2, &fd3, &buf, 0 /*size*/,
	    1 /*set_filter*/);

	/* Generate some traffic to bump the statistics. */
	count = test94_fill_exact(fd2, buf, size, 0);
	test94_fill_exact(fd2, buf, size, 0);
	test94_fill_exact(fd2, buf, size, 0);

	if (write(fd3, "X", 1) != 1) e(0);

	if (sysctl(mib, len, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);

	/* Add some slack space ourselves to prevent problems with churn. */
	bdesize = oldlen + sizeof(*bde) * 8;
	if ((bde = malloc(bdesize)) == NULL) e(0);

	oldlen = bdesize;
	if (sysctl(mib, len, bde, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen % sizeof(*bde)) e(0);

	found = 0;
	for (slot = 0; slot < oldlen / sizeof(*bde); slot++) {
		if (bde[slot].bde_pid != getpid())
			continue;

		if (bde[slot].bde_bufsize != size) e(0);
		if (bde[slot].bde_promisc != 0) e(0);
		if (bde[slot].bde_state != BPF_IDLE) e(0);
		if (bde[slot].bde_immediate != 0) e(0);
		if (bde[slot].bde_hdrcmplt != 0) e(0);
		if (bde[slot].bde_seesent != 1) e(0);
		if (bde[slot].bde_rcount < count * 3 + 1) e(0);
		if (bde[slot].bde_dcount != count) e(0);
		if (bde[slot].bde_ccount != count * 3) e(0);
		if (strcmp(bde[slot].bde_ifname, LOOPBACK_IFNAME) != 0) e(0);

		found++;
	}
	if (found != 1) e(0);

	/*
	 * If global statistics are an accumulation of individual devices'
	 * statistics (they currently are not) then such a scheme should take
	 * into account device flushes.
	 */
	if (ioctl(fd, BIOCFLUSH) != 0) e(0);

	test94_cleanup(fd, fd2, fd3, buf);

	/*
	 * Now see if the global statistics have indeed changed correctly.
	 */
	oldlen = sizeof(bs2);
	if (sysctl(smib, __arraycount(smib), &bs2, &oldlen, NULL, 0) != 0)
		e(0);
	if (oldlen != sizeof(bs2)) e(0);

	if (bs2.bs_recv < bs1.bs_recv + count * 3 + 1) e(0);
	if (bs2.bs_drop != bs1.bs_drop + count) e(0);
	if (bs2.bs_capt != bs1.bs_capt + count * 3) e(0);

	/*
	 * Check an unconfigured BPF device as well.
	 */
	if ((fd = open(_PATH_BPF, O_RDWR)) < 0) e(0);

	/*
	 * Toggle some flags.  It is too much effort to test them all
	 * individually (which, in the light of copy-paste mistakes, would be
	 * the right thing to do) but at least we'll know something gets set.
	 */
	uval = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &uval) != 0) e(0);
	if (ioctl(fd, BIOCSHDRCMPLT, &uval) != 0) e(0);

	uval = 0;
	if (ioctl(fd, BIOCSSEESENT, &uval) != 0) e(0);

	oldlen = bdesize;
	if (sysctl(mib, len, bde, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen % sizeof(*bde)) e(0);

	found = 0;
	for (slot = 0; slot < oldlen / sizeof(*bde); slot++) {
		if (bde[slot].bde_pid != getpid())
			continue;

		if (bde[slot].bde_bufsize != size) e(0);
		if (bde[slot].bde_promisc != 0) e(0);
		if (bde[slot].bde_state != BPF_IDLE) e(0);
		if (bde[slot].bde_immediate != 1) e(0);
		if (bde[slot].bde_hdrcmplt != 1) e(0);
		if (bde[slot].bde_seesent != 0) e(0);
		if (bde[slot].bde_rcount != 0) e(0);
		if (bde[slot].bde_dcount != 0) e(0);
		if (bde[slot].bde_ccount != 0) e(0);
		if (bde[slot].bde_ifname[0] != '\0') e(0);

		found++;
	}
	if (found != 1) e(0);

	close(fd);

	/*
	 * At this point there should be no BPF device left for our PID.
	 */
	oldlen = bdesize;
	if (sysctl(mib, len, bde, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen % sizeof(*bde)) e(0);

	for (slot = 0; slot < oldlen / sizeof(*bde); slot++)
		if (bde[slot].bde_pid == getpid()) e(0);
			found++;

	free(bde);
}

/*
 * Test privileged operations as an unprivileged caller.
 */
static void
test94k(void)
{
	struct passwd *pw;
	pid_t pid;
	size_t len, oldlen;
	int mib[5], status;

	subtest = 11;

	pid = fork();
	switch (pid) {
	case 0:
		errct = 0;

		if ((pw = getpwnam(NONROOT_USER)) == NULL) e(0);

		if (setuid(pw->pw_uid) != 0) e(0);

		/*
		 * Opening /dev/bpf must fail.  Note that this is a system
		 * configuration issue rather than a LWIP service issue.
		 */
		if (open(_PATH_BPF, O_RDWR) != -1) e(0);
		if (errno != EACCES) e(0);

		/*
		 * Retrieving the net.bpf.peers list must fail, too.
		 */
		memset(mib, 0, sizeof(mib));
		len = __arraycount(mib);
		if (sysctlnametomib("net.bpf.peers", mib, &len) != 0) e(0);
		if (len != 3) e(0);
		mib[len++] = sizeof(struct bpf_d_ext);
		mib[len++] = INT_MAX;

		if (sysctl(mib, len, NULL, &oldlen, NULL, 0) != -1) e(0);
		if (errno != EPERM) e(0);

		exit(errct);
	case -1:
		e(0);

		break;
	default:
		break;
	}

	if (wait(&status) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);
}

/*
 * Test that traffic directed to loopback addresses be dropped on non-loopback
 * interfaces.  In particular, inbound traffic to 127.0.0.1 and ::1 should not
 * be accepted on any interface that does not own those addresses.  This test
 * is here because BPF feedback mode is (currently) the only way in which we
 * can generate inbound traffic the ethernet level, and even then only as a
 * side effect of sending outbound traffic.  That is: this test sends the same
 * test packets to the local network!  As such it must be performed only when
 * USENETWORK=yes and therefore at the user's risk.
 */
static void
test94l(void)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_dl sdl;
	struct ifreq ifr;
	struct ifaddrs *ifa, *ifp;
	struct if_data *ifdata;
	uint8_t buf[sizeof(struct ether_header) + MAX(sizeof(struct ip),
	    sizeof(struct ip6_hdr)) + sizeof(struct udphdr) + 6];
	struct ether_header ether;
	const uint8_t ether_src[ETHER_ADDR_LEN] =
	    { 0x02, 0x00, 0x01, 0x12, 0x34, 0x56 };
	unsigned int val;
	size_t off;
	int bfd, sfd;

	subtest = 12;

	if (!get_setting_use_network())
		return;

	memset(&ifr, 0, sizeof(ifr));
	memset(&ether, 0, sizeof(ether));

	/*
	 * Start by finding a suitable ethernet interface that is up and of
	 * which the link is not down.  Without one, we cannot perform this
	 * test.  Save the interface name and the ethernet address.
	 */
	if (getifaddrs(&ifa) != 0) e(0);

	for (ifp = ifa; ifp != NULL; ifp = ifp->ifa_next) {
		if (!(ifp->ifa_flags & IFF_UP) || ifp->ifa_addr == NULL ||
		    ifp->ifa_addr->sa_family != AF_LINK)
			continue;

		ifdata = (struct if_data *)ifp->ifa_data;
		if (ifdata != NULL && ifdata->ifi_type == IFT_ETHER &&
		    ifdata->ifi_link_state != LINK_STATE_DOWN) {
			strlcpy(ifr.ifr_name, ifp->ifa_name,
			    sizeof(ifr.ifr_name));

			memcpy(&sdl, (struct sockaddr_dl *)ifp->ifa_addr,
			    offsetof(struct sockaddr_dl, sdl_data));
			if (sdl.sdl_alen != sizeof(ether.ether_dhost)) e(0);
			memcpy(ether.ether_dhost,
			    ((struct sockaddr_dl *)ifp->ifa_addr)->sdl_data +
			    sdl.sdl_nlen, sdl.sdl_alen);
			break;
		}
	}

	freeifaddrs(ifa);

	if (ifp == NULL)
		return;

	/* Open a BPF device and bind it to the ethernet interface we found. */
	if ((bfd = open(_PATH_BPF, O_RDWR)) < 0) e(0);

	if (ioctl(bfd, BIOCSETIF, &ifr) != 0) e(0);

	if (ioctl(bfd, BIOCGDLT, &val) != 0) e(0);
	if (val != DLT_EN10MB) e(0);

	val = 1;
	if (ioctl(bfd, BIOCSFEEDBACK, &val) != 0) e(0);

	/* We use UDP traffic for our test packets, IPv4 first. */
	if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(TEST_PORT_B);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) != 0) e(0);

	/*
	 * Construct and send a packet.  We already filled in the ethernet
	 * destination address.  Put in a source address that is locally
	 * administered but valid (and as such no reason for packet rejection).
	 */
	memcpy(ether.ether_shost, ether_src, sizeof(ether.ether_shost));
	ether.ether_type = htons(ETHERTYPE_IP);

	memcpy(buf, &ether, sizeof(ether));
	off = sizeof(ether);
	off += test94_make_pkt(buf + off, 6, 0 /*v6*/);
	if (off + 6 > sizeof(buf)) e(0);
	memcpy(buf + off, "Hello!", 6);

	if (write(bfd, buf, off + 6) != off + 6) e(0);

	/* The packet MUST NOT arrive. */
	if (recv(sfd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (close(sfd) != 0) e(0);

	/* Try the same thing, but now with an IPv6 packet. */
	if ((sfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) e(0);

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(TEST_PORT_B);
	memcpy(&sin6.sin6_addr, &in6addr_loopback, sizeof(sin6.sin6_addr));
	if (bind(sfd, (struct sockaddr *)&sin6, sizeof(sin6)) != 0) e(0);

	ether.ether_type = htons(ETHERTYPE_IPV6);

	memcpy(buf, &ether, sizeof(ether));
	off = sizeof(ether);
	off += test94_make_pkt(buf + off, 6, 1 /*v6*/);
	if (off + 6 > sizeof(buf)) e(0);
	memcpy(buf + off, "Hello!", 6);

	if (write(bfd, buf, off + 6) != off + 6) e(0);

	if (recv(sfd, buf, sizeof(buf), MSG_DONTWAIT) != -1) e(0);
	if (errno != EWOULDBLOCK) e(0);

	if (close(sfd) != 0) e(0);
	if (close(bfd) != 0) e(0);
}

/*
 * Test program for LWIP BPF.
 */
int
main(int argc, char ** argv)
{
	int i, m;

	start(94);

	srand48(time(NULL));

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x001) test94a();
		if (m & 0x002) test94b();
		if (m & 0x004) test94c();
		if (m & 0x008) test94d();
		if (m & 0x010) test94e();
		if (m & 0x020) test94f();
		if (m & 0x040) test94g();
		if (m & 0x080) test94h();
		if (m & 0x100) test94i();
		if (m & 0x200) test94j();
		if (m & 0x400) test94k();
		if (m & 0x800) test94l();
	}

	quit();
	/* NOTREACHED */
}
