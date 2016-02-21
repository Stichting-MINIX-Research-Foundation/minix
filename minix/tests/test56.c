/*
 * Test Program for Unix Domain Sockets
 *
 * Overview: This program tests Unix Domain Sockets. It attempts
 * to exercise the functions associated with Unix Domain Sockets.
 * It also attempts to make sure all of the functions which handle
 * file/socket descriptors work correctly when given a socket
 * descriptor for a Unix domain socket. It also implicitly checks
 * for the existance of constants like AF_UNIX and structures like
 * sockaddr_un (it won't compile if they aren't defined). Besides
 * checking that the sockets work properly, this test program also
 * checks that the errors returned conform to the POSIX 2008
 * standards. Some tests are omitted as they could adversely affect
 * the operation of the host system. For example, implementing a test
 * for socket() failing with errno = ENFILE would require using up all
 * of the file descriptors supported by the OS (defined in
 * /proc/sys/fs/file-max on Linux); this could cause problems for
 * daemons and other processes running on the system. Some tests are
 * omitted because they would require changes to libc or the kernel.
 * For example, getting EINTR would require  delaying the system call
 * execution time long enough to raise a signal to interupt it. Some
 * tests were omitted because the particular errors cannot occur when
 * using Unix domain sockets. For example, write() will never fail with
 * ENETDOWN because Unix domain sockets don't use network interfaces.
 *
 * Structure: Some functions can be tested or partially tested without
 * making a connection, socket() for example. These have test
 * functions like test_NAME(). The functionality that needs two way
 * communication is contained within test_xfer().
 *
 * Functions Tested: accept(), bind(), close(), connect(), dup(),
 * dup2(), fstat(), getpeername(), getsockname(), getsockopt(),
 * listen(), read(), readv(), recv(), recvfrom(), recvmsg(), select(), 
 * send(), sendmsg(), sendto(), setsockopt(), shutdown(), socket(), 
 * socketpair(), write(), writev()
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ucred.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Maximum number of errors that we'll allow to occur before this test 
 * program gives us and quits.
 */
int max_error = 4;
#include "common.h"
#include "common-socket.h"


/* Use the common testing code instead of reinventing the wheel. */

/* path of the unix domain socket */
#define TEST_SUN_PATH "test.sock"
#define TEST_SUN_PATHB "testb.sock"

/* filenames for symlinks -- we link these to each other to test ELOOP .*/
#define TEST_SYM_A "test.a"
#define TEST_SYM_B "test.b"

/* text file and test phrase for testing file descriptor passing */
#define TEST_TXT_FILE "test.txt"
#define MSG "This raccoon loves to eat bugs.\n"

/* socket types supported */
static int types[3] = {SOCK_STREAM, SOCK_SEQPACKET, SOCK_DGRAM};

static void test_header(void)
{
	struct sockaddr_un sun;
	debug("entering test_header()");

	sun.sun_family = AF_UNIX;
	sun.sun_path[0] = 'x';
	sun.sun_path[1] = 'x';
	sun.sun_path[2] = 'x';
	sun.sun_path[3] = '\0';

	if (SUN_LEN(&sun) != 5) {
		test_fail("SUN_LEN(&sun) should be 5");
	}

	if (PF_UNIX != PF_LOCAL || PF_UNIX != AF_UNIX) {
		test_fail("PF_UNIX, PF_LOCAL and AF_UNIX");
	}
}

static void test_socketpair(void)
{
	char buf[128];
	struct sockaddr_un addr;
	int socket_vector[2];
	int rc;
	int i;

	debug("entering test_socketpair()");

	UNLINK(TEST_SUN_PATH);
	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);

	debug("Testing socketpair() success");

	rc = socketpair(PF_UNIX, SOCK_STREAM, 0, socket_vector);
	if (rc == -1) {
		test_fail("socketpair() should have worked");
	}

	debug("Testing a simple read/write using sockets from socketpair()");
	memset(buf, '\0', sizeof(buf));

	strncpy(buf, "Howdy Partner", sizeof(buf) - 1);

	rc = write(socket_vector[0], buf, sizeof(buf));
	if (rc == -1) {
		test_fail("write(sd, buf, sizeof(buf)) failed unexpectedly");
	}

	memset(buf, '\0', sizeof(buf));

	rc = read(socket_vector[1], buf, sizeof(buf));
	if (rc == -1) {
		test_fail("read() failed unexpectedly");
	}

	if (strncmp(buf, "Howdy Partner", strlen("Howdy Partner")) != 0) {
		test_fail("We did not read what we wrote");
	}

	CLOSE(socket_vector[0]);
	CLOSE(socket_vector[1]);

	debug("Test socketpair() with all FDs open by this process");

	for (i = 3; i < getdtablesize(); i++) {
		rc = open("/dev/null", O_RDONLY);
		if (rc == -1) {
			test_fail("we couldn't open /dev/null for read");
		}
	}

	rc = socketpair(PF_UNIX, SOCK_STREAM, 0, socket_vector);
	if (!(rc == -1 && errno == EMFILE)) {
		test_fail("socketpair() should have failed with EMFILE");
	}

	for (i = 3; i < getdtablesize(); i++) {
		CLOSE(i);
	}

	rc = socketpair(PF_UNIX, SOCK_STREAM, 4, socket_vector);
	if (!(rc == -1 && errno == EPROTONOSUPPORT)) {
		test_fail("socketpair() should have failed");
	}

	debug("leaving test_socketpair()");
}

static void test_ucred(void)
{
	struct unpcbid credentials;
	socklen_t ucred_length;
	uid_t euid = geteuid();
	gid_t egid = getegid();
	int sv[2];
	int rc;

	debug("Test peer credentials");

	ucred_length = sizeof(credentials);

	rc = socketpair(PF_UNIX, SOCK_STREAM, 0, sv);
	if (rc == -1) {
		test_fail("socketpair(PF_UNIX, SOCK_STREAM, 0, sv) failed");
	}

	memset(&credentials, '\0', ucred_length);
	rc = getsockopt(sv[0], 0, LOCAL_PEEREID, &credentials,
							&ucred_length);
	if (rc == -1) {
		test_fail("getsockopt(LOCAL_PEEREID) failed");
	} else if (credentials.unp_pid != getpid() ||
			credentials.unp_euid != geteuid() ||
			credentials.unp_egid != getegid()) {
		printf("%d=%d %d=%d %d=%d",credentials.unp_pid, getpid(),
		    credentials.unp_euid, geteuid(),
		    credentials.unp_egid, getegid());
		test_fail("Credential passing gave us the wrong cred");
	}

	rc = getpeereid(sv[0], &euid, &egid);
	if (rc == -1) {
		test_fail("getpeereid(sv[0], &euid, &egid) failed");
	} else if (credentials.unp_euid != euid ||
	    credentials.unp_egid != egid) {
		test_fail("getpeereid() didn't give the correct euid/egid");
	}

	CLOSE(sv[0]);
	CLOSE(sv[1]);
}

static void callback_check_sockaddr(const struct sockaddr *sockaddr,
	socklen_t sockaddrlen, const char *callname, int addridx) {
	char buf[256];
	const char *path;
	const struct sockaddr_un *sockaddr_un =
		(const struct sockaddr_un *) sockaddr;

	switch (addridx) {
	case 1: path = TEST_SUN_PATH; break;
	case 2: path = TEST_SUN_PATHB; break;
	default:
		fprintf(stderr, "error: invalid addridx %d in "
			"callback_check_sockaddr\n", addridx);
		abort();
	}

	if (!(sockaddr_un->sun_family == AF_UNIX &&
			strncmp(sockaddr_un->sun_path,
			path,
			sizeof(sockaddr_un->sun_path) - 1) == 0)) {

		snprintf(buf, sizeof(buf), "%s() didn't return the right addr",
			callname);
		test_fail(buf);
		fprintf(stderr, "exp: '%s' | got: '%s'\n", path,
			sockaddr_un->sun_path);
	}
}

static void callback_cleanup(void) {
	UNLINK(TEST_SUN_PATH);
	UNLINK(TEST_SUN_PATHB);
	UNLINK(TEST_SYM_A);
	UNLINK(TEST_SYM_B);
}

static void test_bind_unix(void)
{
	struct sockaddr_un addr;
	int sd;
	int rc;

	debug("entering test_bind_unix()");
	UNLINK(TEST_SUN_PATH);
	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);

	debug("Test bind() with an empty sun_path");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	memset(addr.sun_path, '\0', sizeof(addr.sun_path));
	errno = 0;

	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (!(rc == -1 && errno == ENOENT)) {
		test_fail("bind() should have failed with ENOENT");
	}
	CLOSE(sd);

	debug("Test bind() using a symlink loop");

	UNLINK(TEST_SUN_PATH);
	UNLINK(TEST_SYM_A);
	UNLINK(TEST_SYM_B);

	SYMLINK(TEST_SYM_B, TEST_SYM_A);

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	strncpy(addr.sun_path, TEST_SYM_A, sizeof(addr.sun_path) - 1);
	errno = 0;
	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (!((rc == -1) && (errno == EADDRINUSE))) {
		test_fail("bind() should have failed with EADDRINUSE");
	}
	CLOSE(sd);

	SYMLINK(TEST_SYM_A, TEST_SYM_B);

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	strncpy(addr.sun_path, TEST_SYM_A, sizeof(addr.sun_path) - 1);
	strlcat(addr.sun_path, "/x", sizeof(addr.sun_path));
	errno = 0;
	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (!((rc == -1) && (errno == ELOOP))) {
		test_fail("bind() should have failed with ELOOP");
	}
	CLOSE(sd);

	UNLINK(TEST_SUN_PATH);
	UNLINK(TEST_SYM_A);
	UNLINK(TEST_SYM_B);

	/* Test bind with garbage in sockaddr_un */
	memset(&addr, '?', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 'f';
	addr.sun_path[1] = 'o';
	addr.sun_path[2] = 'o';
	addr.sun_path[3] = '\0';
	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	rc = bind(sd, (struct sockaddr *) &addr,
	    offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) +
	    1);
	if (rc == -1) {
		test_fail("bind() should have worked");
	}
	CLOSE(sd);
	UNLINK("foo");

	debug("leaving test_bind_unix()");
}

static void callback_xfer_prepclient(void) {
	debug("Creating symlink to TEST_SUN_PATH");

	SYMLINK(TEST_SUN_PATH, TEST_SYM_A);
}

static void callback_xfer_peercred(int sd) {
	struct unpcbid credentials;
	int rc;
	socklen_t ucred_length;

	ucred_length = sizeof(credentials);

	debug("Test obtaining the peer credentials");

	memset(&credentials, '\0', ucred_length);
	rc = getsockopt(sd, 0, LOCAL_PEEREID, &credentials, &ucred_length);

	if (rc == -1) {
		test_fail("[client] getsockopt() failed");
	} else if (credentials.unp_euid != geteuid() ||
	    credentials.unp_egid != getegid()) {
		printf("%d=* %d=%d %d=%d", credentials.unp_pid,
		    credentials.unp_euid, geteuid(),
		    credentials.unp_egid, getegid());
		test_fail("[client] Credential passing gave us a bad UID/GID");
	}
}

static void
callback_set_listen_opt(int sd)
{
	int val;

	/*
	 * Several of the tests assume that a new connection to a server will
	 * not be established (i.e., go from "connecting" to "connected" state)
	 * until the server actually accepts the connection with an accept(2)
	 * call.  With the new UDS implementation, this is no longer true: to
	 * match the behavior of other systems, UDS now preemptively connects
	 * the socket in anticipation of the accept(2) call.  We can change
	 * back to the old behavior by setting LOCAL_CONNWAIT however, and
	 * since the test effectively tests a larger set of socket transitions
	 * that way, that is what we do for these tests.
	 */
	val = 1;
	if (setsockopt(sd, 0, LOCAL_CONNWAIT, &val, sizeof(val)) != 0)
		test_fail("setsockopt(LOCAL_CONNWAIT)");
}

static void test_vectorio(int type)
{
	int sv[2];
	int rc;
	struct iovec iov[3];
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	char buf3[BUFSIZE];
	char buf4[BUFSIZE*3];
	const struct iovec *iovp = iov;

	debug("begin vectorio tests");

	memset(buf1, '\0', BUFSIZE);
	strncpy(buf1, "HELLO ", BUFSIZE - 1);

	memset(buf2, '\0', BUFSIZE);
	strncpy(buf2, "WORLD", BUFSIZE - 1);

	memset(buf3, '\0', BUFSIZE);

	rc = socketpair(PF_UNIX, type, 0, sv);
	if (rc == -1) {
		test_fail("socketpair");
	}

	iov[0].iov_base = buf1;
	iov[0].iov_len  = strlen(buf1);
	iov[1].iov_base = buf2;
	iov[1].iov_len  = strlen(buf2);
	iov[2].iov_base = buf3;
	iov[2].iov_len  = 1;

	rc = writev(sv[0], iovp, 3);
	if (rc == -1) {
		test_fail("writev");
	}

	memset(buf4, '\0', BUFSIZE*3);

	rc = read(sv[1], buf4, BUFSIZE*3);
	if (rc == -1) {
		test_fail("read");
	}

	if (strncmp(buf4, "HELLO WORLD", strlen("HELLO WORLD"))) {
		test_fail("the string we read was not 'HELLO WORLD'");
	}

	memset(buf1, '\0', BUFSIZE);
	strncpy(buf1, "Unit Test Time", BUFSIZE - 1);

	rc = write(sv[1], buf1, strlen(buf1) + 1);
	if (rc == -1) {
		test_fail("write");
	}

	memset(buf2, '\0', BUFSIZE);
	memset(buf3, '\0', BUFSIZE);
	memset(buf4, '\0', BUFSIZE*3);

	iov[0].iov_base = buf2;
	iov[0].iov_len  = 5;
	iov[1].iov_base = buf3;
	iov[1].iov_len  = 5;
	iov[2].iov_base = buf4;
	iov[2].iov_len  = 32;

	rc = readv(sv[0], iovp, 3);
	if (rc == -1) {
		test_fail("readv");
	}

	if (strncmp(buf2, "Unit ", 5) || strncmp(buf3, "Test ", 5) || 
					strncmp(buf4, "Time", 4)) {
		test_fail("readv");
	}

	rc = close(sv[0]);
	if (rc == -1) {
		test_fail("close");
	}

	rc = close(sv[1]);
	if (rc == -1) {
		test_fail("close");
	}

	debug("done vector io tests");
}

static void test_msg(int type)
{
	int sv[2];
	int rc;
	struct msghdr msg1;
	struct msghdr msg2;
	struct iovec iov[3];
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	char buf3[BUFSIZE];
	char buf4[BUFSIZE*3];

	debug("begin sendmsg/recvmsg tests");

	memset(buf1, '\0', BUFSIZE);
	strncpy(buf1, "HELLO ", BUFSIZE - 1);

	memset(buf2, '\0', BUFSIZE);
	strncpy(buf2, "WORLD", BUFSIZE - 1);

	memset(buf3, '\0', BUFSIZE);

	rc = socketpair(PF_UNIX, type, 0, sv);
	if (rc == -1) {
		test_fail("socketpair");
	}

	iov[0].iov_base = buf1;
	iov[0].iov_len  = strlen(buf1);
	iov[1].iov_base = buf2;
	iov[1].iov_len  = strlen(buf2);
	iov[2].iov_base = buf3;
	iov[2].iov_len  = 1;

	memset(&msg1, '\0', sizeof(struct msghdr));
	msg1.msg_name = NULL;
	msg1.msg_namelen = 0;
	msg1.msg_iov = iov;
	msg1.msg_iovlen = 3;
	msg1.msg_control = NULL;
	msg1.msg_controllen = 0;
	msg1.msg_flags = 0;

	rc = sendmsg(sv[0], &msg1, 0);
	if (rc == -1) {
		test_fail("writev");
	}

	memset(buf4, '\0', BUFSIZE*3);

	rc = read(sv[1], buf4, BUFSIZE*3);
	if (rc == -1) {
		test_fail("read");
	}

	if (strncmp(buf4, "HELLO WORLD", strlen("HELLO WORLD"))) {
		test_fail("the string we read was not 'HELLO WORLD'");
	}

	memset(buf1, '\0', BUFSIZE);
	strncpy(buf1, "Unit Test Time", BUFSIZE - 1);

	rc = write(sv[1], buf1, strlen(buf1) + 1);
	if (rc == -1) {
		test_fail("write");
	}

	memset(buf2, '\0', BUFSIZE);
	memset(buf3, '\0', BUFSIZE);
	memset(buf4, '\0', BUFSIZE*3);

	iov[0].iov_base = buf2;
	iov[0].iov_len  = 5;
	iov[1].iov_base = buf3;
	iov[1].iov_len  = 5;
	iov[2].iov_base = buf4;
	iov[2].iov_len  = 32;

	memset(&msg2, '\0', sizeof(struct msghdr));
	msg2.msg_name = NULL;
	msg2.msg_namelen = 0;
	msg2.msg_iov = iov;
	msg2.msg_iovlen = 3;
	msg2.msg_control = NULL;
	msg2.msg_controllen = 0;
	msg2.msg_flags = 0;

	rc = recvmsg(sv[0], &msg2, 0);
	if (rc == -1) {
		test_fail("readv");
	}

	if (strncmp(buf2, "Unit ", 5) || strncmp(buf3, "Test ", 5) || 
					strncmp(buf4, "Time", 4)) {
		test_fail("readv");
	}

	rc = close(sv[0]);
	if (rc == -1) {
		test_fail("close");
	}

	rc = close(sv[1]);
	if (rc == -1) {
		test_fail("close");
	}
}

static void test_scm_credentials(void)
{
	int rc;
	int src;
	int dst;
	int one;
	union {
		struct sockcred cred;
		char buf[SOCKCREDSIZE(NGROUPS_MAX)];
	} cred;
	struct cmsghdr *cmsg = NULL;
	struct sockaddr_un addr;
	struct iovec iov[3];
	struct msghdr msg1;
	struct msghdr msg2;
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	char buf3[BUFSIZE];
	char ctrl[BUFSIZE];
	socklen_t len, addrlen = sizeof(struct sockaddr_un);

	debug("test_scm_credentials");

	UNLINK(TEST_SUN_PATH);
	UNLINK(TEST_SUN_PATHB);

	debug("creating src socket");

	src = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (src == -1) {
		test_fail("socket");
	}

	debug("creating dst socket");

	dst = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (dst == -1) {
		test_fail("socket");
	}

	debug("binding src socket");

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATHB, sizeof(addr.sun_path) - 1);
	rc = bind(src, (struct sockaddr *) &addr, addrlen);
	if (rc == -1) {
		test_fail("bind");
	}

	debug("binding dst socket");

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);

	rc = bind(dst, (struct sockaddr *) &addr, addrlen);
	if (rc == -1) {
		test_fail("bind");
	}

	debug("request credential passing");

	one = 1;
	rc = setsockopt(dst, 0, LOCAL_CREDS, &one, sizeof(one));
	if (rc == -1) {
		test_fail("setsockopt(LOCAL_CREDS)");
	}

	debug("sending msg1");

	memset(&buf1, '\0', BUFSIZE);
	memset(&buf2, '\0', BUFSIZE);
	memset(&buf3, '\0', BUFSIZE);
	memset(&ctrl, '\0', BUFSIZE);

	strncpy(buf1, "Minix ", BUFSIZE-1);
	strncpy(buf2, "is ", BUFSIZE-1);
	strncpy(buf3, "great!", BUFSIZE-1);

	iov[0].iov_base = buf1;
	iov[0].iov_len  = 6;
	iov[1].iov_base = buf2;
	iov[1].iov_len  = 3;
	iov[2].iov_base = buf3;
	iov[2].iov_len  = 32;

	memset(&msg1, '\0', sizeof(struct msghdr));
	msg1.msg_name = &addr;
	msg1.msg_namelen = addrlen;
	msg1.msg_iov = iov;
	msg1.msg_iovlen = 3;
	msg1.msg_control = NULL;
	msg1.msg_controllen = 0;
	msg1.msg_flags = 0;

	rc = sendmsg(src, &msg1, 0);
	if (rc == -1) {
		test_fail("sendmsg");
	}

	memset(&buf1, '\0', BUFSIZE);
	memset(&buf2, '\0', BUFSIZE);
	memset(&buf3, '\0', BUFSIZE);
	memset(&ctrl, '\0', BUFSIZE);

	iov[0].iov_base = buf1;
	iov[0].iov_len  = 9;
	iov[1].iov_base = buf2;
	iov[1].iov_len  = 32;

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	memset(&msg2, '\0', sizeof(struct msghdr));
	msg2.msg_name = &addr;
	msg2.msg_namelen = sizeof(struct sockaddr_un);
	msg2.msg_iov = iov;
	msg2.msg_iovlen = 2;
	msg2.msg_control = ctrl;
	msg2.msg_controllen = BUFSIZE;
	msg2.msg_flags = 0;

	debug("recv msg2");

	rc = recvmsg(dst, &msg2, 0);
	if (rc == -1) {
		test_fail("recvmsg");
	}

	debug("checking results");

	if (strncmp(buf1, "Minix is ", 9) || strncmp(buf2, "great!", 6)) {
		test_fail("recvmsg");
	}

	/* we need to use the full path "/usr/src/test/DIR_56/testb.sock"
	 * because that is what is returned by recvmsg().
	 */
	if (addr.sun_family != AF_UNIX || strcmp(addr.sun_path,
					TEST_SUN_PATHB)) {
		test_fail("recvmsg");
	}

	debug("looking for credentials");

	len = 0;

	memset(&cred, 'x', sizeof(cred));
	for (cmsg = CMSG_FIRSTHDR(&msg2); cmsg != NULL;
					cmsg = CMSG_NXTHDR(&msg2, cmsg)) {

		if (cmsg->cmsg_level == SOL_SOCKET &&
				cmsg->cmsg_type == SCM_CREDS) {
			/* Great, this alignment business!  But then at least
			 * give me a macro to compute the actual data length..
			 */
			len = cmsg->cmsg_len - (socklen_t)
			    ((char *)CMSG_DATA(cmsg) - (char *)cmsg);

			if (len < sizeof(struct sockcred))
				test_fail("credentials too small");
			else if (len > sizeof(cred))
				test_fail("credentials too large");
			memcpy(cred.buf, CMSG_DATA(cmsg), len);
			break;
		}
	}

	if (len == 0)
		test_fail("no credentials found");

	if (len != SOCKCREDSIZE(cred.cred.sc_ngroups))
		test_fail("wrong credentials size");

	/*
	 * TODO: check supplementary groups.  This whole test is pretty much
	 * pointless since we're running with very standard credentials anyway.
	 */
	if (cred.cred.sc_uid != getuid() ||
	    cred.cred.sc_euid != geteuid() ||
	    cred.cred.sc_gid != getgid() ||
	    cred.cred.sc_egid != getegid() ||
	    cred.cred.sc_ngroups < 0 || cred.cred.sc_ngroups > NGROUPS_MAX) {
		test_fail("did no receive the proper credentials");
	}

	rc = close(dst);
	if (rc == -1) {
		test_fail("close");
	}

	rc = close(src);
	if (rc == -1) {
		test_fail("close");
	}

	UNLINK(TEST_SUN_PATH);
	UNLINK(TEST_SUN_PATHB);
}

static void test_connect(const struct socket_test_info *info)
{
	int i, sd, sds[2], rc;

	/* connect() is already tested throughout test56, but
	 * in most cases the client and server end up on /dev/uds
	 * minor 0 and minor 1. This test opens some sockets first and 
	 * then calls test_simple_client_server(). This forces the 
	 * client and server minor numbers higher in the descriptor table.
	 */

	debug("starting test_connect()");

	sd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sd == -1) {
		test_fail("couldn't create a socket");
	}

	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sds);
	if (rc == -1) {
		test_fail("couldn't create a socketpair");
	}

	for (i = 0; i < 3; i++) {
		test_simple_client_server(info, types[i]);
	}

	rc = close(sds[1]);
	if (rc == -1) {
		test_fail("close() failed");
	}

	rc = close(sds[0]);
	if (rc == -1) {
		test_fail("close() failed");
	}

	rc = close(sd);
	if (rc == -1) {
		test_fail("close() failed");
	}

	debug("exiting test_connect()");
}

static int test_multiproc_read(void)
{
/* test that when we fork() a process with an open socket descriptor, 
 * the descriptor in each process points to the same thing.
 */

	pid_t pid;
	int sds[2];
	int rc, status;
	char buf[3];

	debug("entering test_multiproc_read()");

	rc = socketpair(PF_UNIX, SOCK_STREAM, 0, sds);
	if (rc == -1) {
		test_fail("socketpair");
		return 1;
	}

	memset(buf, '\0', 3);


	/* the signal handler is only used by the client, but we have to 
	 * install it now. if we don't the server may signal the client 
	 * before the handler is installed.
	 */
	debug("installing signal handler");
	if (signal(SIGUSR1, test_xfer_sighdlr) == SIG_ERR) {
		test_fail("signal(SIGUSR1, test_xfer_sighdlr) failed");
		return 1;
	}

	debug("signal handler installed");

	server_ready = 0;

	pid = fork();

	if (pid == -1) {

		test_fail("fork");
		return 1;

	} else if (pid == 0) {

		while (server_ready == 0) {
			debug("waiting for SIGUSR1 from parent");
			sleep(1);
		}

		rc = read(sds[1], buf, 2);
		if (rc == -1) {
			test_fail("read");
			exit(1);
		}

		if (!(buf[0] == 'X' && buf[1] == '3')) {
			test_fail("Didn't read X3");
			exit(1);
		}

		exit(0);
	} else {

		rc = write(sds[0], "MNX3", 4);
		if (rc == -1) {
			test_fail("write");
		}

		rc = read(sds[1], buf, 2);
		if (rc == -1) {
			test_fail("read");
		}

		if (!(buf[0] == 'M' && buf[1] == 'N')) {
			test_fail("Didn't read MN");
		}

		/* time to tell the client to start the test */
		kill(pid, SIGUSR1);

		do {
			rc = waitpid(pid, &status, 0);
		} while (rc == -1 && errno == EINTR);

		/* we use the exit status to get its error count */
		errct += WEXITSTATUS(status);
	}

	return 0;
}

static int test_multiproc_write(void)
{
/* test that when we fork() a process with an open socket descriptor, 
 * the descriptor in each process points to the same thing.
 */

	pid_t pid;
	int sds[2];
	int rc, status;
	char buf[7];

	debug("entering test_multiproc_write()");

	rc = socketpair(PF_UNIX, SOCK_STREAM, 0, sds);
	if (rc == -1) {
		test_fail("socketpair");
		return 1;
	}

	memset(buf, '\0', 7);


	/* the signal handler is only used by the client, but we have to 
	 * install it now. if we don't the server may signal the client 
	 * before the handler is installed.
	 */
	debug("installing signal handler");
	if (signal(SIGUSR1, test_xfer_sighdlr) == SIG_ERR) {
		test_fail("signal(SIGUSR1, test_xfer_sighdlr) failed");
		return 1;
	}

	debug("signal handler installed");

	server_ready = 0;

	pid = fork();

	if (pid == -1) {

		test_fail("fork");
		return 1;

	} else if (pid == 0) {

		while (server_ready == 0) {
			debug("waiting for SIGUSR1 from parent");
			sleep(1);
		}

		rc = write(sds[1], "IX3", 3);
		if (rc == -1) {
			test_fail("write");
			exit(1);
		}

		rc = read(sds[0], buf, 6);
		if (rc == -1) {
			test_fail("read");
			exit(1);
		}

		if (strcmp(buf, "MINIX3") != 0) {
			test_fail("didn't read MINIX3");
			exit(1);
		}

		exit(0);
	} else {

		rc = write(sds[1], "MIN", 3);
		if (rc == -1) {
			test_fail("write");
		}

		/* time to tell the client to start the test */
		kill(pid, SIGUSR1);

		do {
			rc = waitpid(pid, &status, 0);
		} while (rc == -1 && errno == EINTR);

		/* we use the exit status to get its error count */
		errct += WEXITSTATUS(status);
	}

	return 0;
}

static void test_fd_passing_child(int sd)
{
	int fd, rc;
	char x = 'x';
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	struct iovec iov;
	char buf[BUFSIZE];

	memset(buf, '\0', BUFSIZE);

	fd = open(TEST_TXT_FILE, O_CREAT|O_TRUNC|O_RDWR);
	if (fd == -1) {
		test_fail("could not open test.txt");
	}

	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;

	iov.iov_base = &x;
	iov.iov_len = 1;
	msghdr.msg_iov = &iov;
	msghdr.msg_iovlen = 1;

	msghdr.msg_control = buf;
	msghdr.msg_controllen = CMSG_SPACE(sizeof(int));

	msghdr.msg_flags = 0;

	cmsg = CMSG_FIRSTHDR(&msghdr);
	cmsg->cmsg_len = CMSG_SPACE(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	((int *) CMSG_DATA(cmsg))[0] = fd;

	rc = sendmsg(sd, &msghdr, 0);
	if (rc == -1) {
		test_fail("could not send message");
	}

	memset(buf, '\0', BUFSIZE);
	rc = read(sd, buf, BUFSIZE);
	if (rc == -1) {
		test_fail("could not read from socket");
	}

	if (strcmp(buf, "done") != 0) {
		test_fail("we didn't read the right message");
	}

	memset(buf, '\0', BUFSIZE);
	rc = lseek(fd, 0, SEEK_SET);
	if (rc == -1) {
		test_fail("could not seek to start of test.txt");
	}

	rc = read(fd, buf, BUFSIZE);
	if (rc == -1) {
		test_fail("could not read from test.txt");
	}

	if (strcmp(buf, MSG) != 0) {
		test_fail("other process didn't write MSG to test.txt");
	}

	rc = close(fd);
	if (rc == -1) {
		test_fail("could not close test.txt");
	}

	rc = close(sd);
	if (rc == -1) {
		test_fail("could not close socket");
	}

	rc = unlink(TEST_TXT_FILE);
	if (rc == -1) {
		test_fail("could not unlink test.txt");
	}

	exit(errct);
}

static void test_fd_passing_parent(int sd)
{
	int rc, fd;
	char x;
	struct msghdr msghdr;
	struct cmsghdr *cmsg;
	struct iovec iov;
	char buf[BUFSIZE];

	memset(buf, '\0', BUFSIZE);

	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;

	iov.iov_base = &x;
	iov.iov_len = 1;
	msghdr.msg_iov = &iov;
	msghdr.msg_iovlen = 1;

	msghdr.msg_iov = &iov;
	msghdr.msg_iovlen = 1;

	msghdr.msg_control = buf;
	msghdr.msg_controllen = BUFSIZE;

	msghdr.msg_flags = 0;

	rc = recvmsg(sd, &msghdr, 0);
	if (rc == -1) {
		test_fail("could not recv message.");
	}

	cmsg = CMSG_FIRSTHDR(&msghdr);
	fd = ((int *) CMSG_DATA(cmsg))[0];

	rc = write(fd, MSG, strlen(MSG));
	if (rc != strlen(MSG)) {
		test_fail("could not write the full message to test.txt");
	}

	rc = close(fd);
	if (rc == -1) {
		test_fail("could not close test.txt");
	}

	memset(buf, '\0', BUFSIZE);
	strcpy(buf, "done");
	rc = write(sd, buf, BUFSIZE);
	if (rc == -1) {
		test_fail("could not write to socket");
	}

	rc = close(sd);
	if (rc == -1) {
		test_fail("could not close socket");
	}
}

static void test_permissions(void) {
	/* Test bind and connect for permission verification
	 *
	 * After creating a UDS socket we change user credentials. At that
	 * point we should not be allowed to bind or connect to the UDS socket
	 */

	pid_t pid;
	int sd, rc, status;
	struct sockaddr_un addr;

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);

	UNLINK(TEST_SUN_PATH);

	pid = fork();
	if (pid < 0) test_fail("unable to fork");
	else if (pid == 0) {
		SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
		if (setuid(999) != 0) test_fail("unable to chance uid");
		rc = bind(sd, (struct sockaddr *) &addr,
				 sizeof(struct sockaddr_un));
		if (rc != -1) {
			test_fail("bind() should not have worked");
		}
		exit(errct);
	} else {
		rc = waitpid(pid, &status, 0);
		errct += WEXITSTATUS(status);
	}

	/* the signal handler is only used by the client, but we have to
	 * install it now. if we don't the server may signal the client
	 * before the handler is installed.
	 */
	debug("installing signal handler");
	if (signal(SIGUSR1, test_xfer_sighdlr) == SIG_ERR) {
		test_fail("signal(SIGUSR1, test_xfer_sighdlr) failed");
	}

	debug("signal handler installed");

	server_ready = 0;

	pid = fork();
	if (pid < 0) test_fail("unable to fork");
	else if (pid == 0) {
		while (server_ready == 0) {
			debug("[client] waiting for the server to signal");
			sleep(1);
		}
		SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
		if (setuid(999) != 0) test_fail("unable to chance uid");
		rc = connect(sd, (struct sockaddr *) &addr,
						sizeof(struct sockaddr_un));
		if (rc != -1)
			test_fail("connect should not have worked");
		exit(errct);
	} else {
		SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
		rc = bind(sd, (struct sockaddr *) &addr,
				 sizeof(struct sockaddr_un));
		if (rc == -1) {
			test_fail("bind() should have worked");
		}

		rc = listen(sd, 8);
		if (rc == -1) {
			test_fail("listen(sd, 8) should have worked");
		}
		kill(pid, SIGUSR1);
		sleep(1);
		CLOSE(sd);

		rc = waitpid(pid, &status, 0);
		errct += WEXITSTATUS(status);
	}

	UNLINK(TEST_SUN_PATH);
}

static void test_fd_passing(void) {
	int status;
	int sv[2];
	pid_t pid;
	int rc;

	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
	if (rc == -1) {
		test_fail("socketpair failed");
	}

	pid = fork();
	if (pid == -1) {
		test_fail("fork() failed");

		rc = close(sv[0]);
		if (rc == -1) {
			test_fail("could not close sv[0]");
		}

		rc = close(sv[1]);
		if (rc == -1) {
			test_fail("could not close sv[1]");
		}

		exit(0);
	} else if (pid == 0) {
		rc = close(sv[0]);
		if (rc == -1) {
			test_fail("could not close sv[0]");
		}

		test_fd_passing_child(sv[1]);
		test_fail("should never get here");
		exit(1);
	} else {
		rc = close(sv[1]);
		if (rc == -1) {
			test_fail("could not close sv[1]");
		}

		test_fd_passing_parent(sv[0]);

		/* wait for client to exit */
		do {
			errno = 0;
			rc = waitpid(pid, &status, 0);
		} while (rc == -1 && errno == EINTR);

		/* we use the exit status to get its error count */
		errct += WEXITSTATUS(status);
	}
}

static void test_select(void)
{
	int nfds = -1;
	int socks[2];
	fd_set readfds, writefds;
	struct timeval tv;
	int res = 0;
	char buf[1];

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

	tv.tv_sec = 2;
	tv.tv_usec = 0;	/* 2 sec time out */

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0) {
		test_fail("Can't open socket pair.");
	}
	FD_SET(socks[0], &readfds);
	nfds = socks[0] + 1;

	/* Close the write end of the socket to generate EOF on read end */
	if ((res = shutdown(socks[1], SHUT_WR)) != 0) {
		test_fail("shutdown failed\n");
	}

	res = select(nfds, &readfds, NULL, NULL, &tv);
	if (res != 1) {
		test_fail("select should've returned 1 ready fd\n");
	}
	if (!(FD_ISSET(socks[0], &readfds))) {
		test_fail("The server didn't respond within 2 seconds");
	}
	/* Now try to read from empty, closed pipe */
	if (read(socks[0], buf, sizeof(buf)) != 0) {
		test_fail("reading from empty, closed pipe should return EOF");
	}

	close(socks[0]);

	/* Try again the other way around: create a socketpair, close the
	 * read end, and try to write. This should cause an EPIPE */
	
	tv.tv_sec = 2;
	tv.tv_usec = 0;	/* 2 sec time out */

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0) {
		test_fail("Can't open socket pair.");
	}
	FD_SET(socks[1], &writefds);
	nfds = socks[1] + 1;

	/* kill the read end of the socket to generate EPIPE on write end */
	if ((res = shutdown(socks[0], SHUT_RD)) != 0) {
		test_fail("shutdown failed\n");
	}

	res = select(nfds, NULL, &writefds, NULL, &tv);
	if (res != 1) {
		test_fail("select should've returned 1 ready fd\n");
	}
	if (!(FD_ISSET(socks[1], &writefds))) {
		test_fail("The server didn't respond within 2 seconds");
	}

	/* Now try to write to closed pipe */
	errno = 0;
	if ((res = write(socks[1], buf, sizeof(buf))) != -1) {
		printf("write res = %d\n", res);
		test_fail("writing to empty, closed pipe should fail");
	}
	if (errno != EPIPE) {
		printf("errno = %d\n", errno);
		test_fail("writing to closed pipe should return EPIPE\n");
	}

	close(socks[1]);
}

static void test_select_close(void)
{
	int res, socks[2];
	fd_set readfds;
	struct timeval tv;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0) {
		test_fail("Can't open socket pair.");
	}

	switch (fork()) {
	case 0:
		sleep(1);

		exit(0);
	case -1:
		test_fail("Can't fork.");
	default:
		break;
	}

	close(socks[1]);

	FD_ZERO(&readfds);
	FD_SET(socks[0], &readfds);
	tv.tv_sec = 2;
	tv.tv_usec = 0;	/* 2 sec time out */

	res = select(socks[0] + 1, &readfds, NULL, NULL, &tv);
	if (res != 1) {
		test_fail("select should've returned 1 ready fd\n");
	}
	if (!(FD_ISSET(socks[0], &readfds))) {
		test_fail("The server didn't respond within 2 seconds");
	}

	wait(NULL);

	close(socks[0]);
}

static void test_fchmod(void)
{
	int socks[2];
	struct stat st1, st2;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) < 0) {
		test_fail("Can't open socket pair.");
	}

	if (fstat(socks[0], &st1) < 0 || fstat(socks[1], &st2) < 0) {
		test_fail("fstat failed.");
	}

	if ((st1.st_mode & (S_IRUSR|S_IWUSR)) == S_IRUSR &&
		(st2.st_mode & (S_IRUSR|S_IWUSR)) == S_IWUSR) {
		test_fail("fstat failed.");
	}

	if (fchmod(socks[0], S_IRUSR) < 0 ||
                    fstat(socks[0], &st1) < 0 ||
                    (st1.st_mode & (S_IRUSR|S_IWUSR)) != S_IRUSR) {
		test_fail("fchmod/fstat mode set/check failed (1).");
	}

	if (fchmod(socks[1], S_IWUSR) < 0 || fstat(socks[1], &st2) < 0 ||
                    (st2.st_mode & (S_IRUSR|S_IWUSR)) != S_IWUSR) {
		test_fail("fchmod/fstat mode set/check failed (2).");
	}

	close(socks[0]);
	close(socks[1]);
}

/*
 * Test various aspects related to the socket files on the file system.
 * This subtest is woefully incomplete and currently only attempts to test
 * aspects that have recently been affected by code changes.  In the future,
 * there should be tests for the entire range of file system path and access
 * related error codes (TODO).
 */
static void
test_file(void)
{
	struct sockaddr_un addr, saddr, saddr2;
	char buf[1];
	socklen_t len;
	struct stat st;
	mode_t omask;
	int sd, sd2, csd, fd;

	/*
	 * If the provided socket path exists on the file system, the bind(2)
	 * call must fail, regardless of whether there is a socket that is
	 * bound to that address.
	 */
	UNLINK(TEST_SUN_PATH);

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path));

	if ((sd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1)
		test_fail("Can't open socket");
	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		test_fail("Can't bind socket");

	if ((sd2 = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1)
		test_fail("Can't open socket");
	if (bind(sd2, (struct sockaddr *)&addr, sizeof(addr)) != -1)
		test_fail("Binding socket unexpectedly succeeded");
	if (errno != EADDRINUSE)
		test_fail("Binding socket failed with wrong error");

	CLOSE(sd);

	if (bind(sd2, (struct sockaddr *)&addr, sizeof(addr)) != -1)
		test_fail("Binding socket unexpectedly succeeded");
	if (errno != EADDRINUSE)
		test_fail("Binding socket failed with wrong error");

	CLOSE(sd2);

	UNLINK(TEST_SUN_PATH);

	/*
	 * If the socket is removed from the file system while there is still a
	 * socket bound to it, it should be possible to bind a new socket to
	 * the address.  The old socket should then become unreachable in terms
	 * of connections and data directed to the address, even though it
	 * should still appear to be bound to the same address.
	 */
	if ((sd = socket(PF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0)) == -1)
		test_fail("Can't open socket");
	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		test_fail("Can't bind socket");
	memset(&saddr, 0, sizeof(saddr));
	len = sizeof(saddr);
	if (getsockname(sd, (struct sockaddr *)&saddr, &len) != 0)
		test_fail("Can't get socket address");

	if ((csd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1)
		test_fail("Can't open client socket");

	memset(buf, 'X', sizeof(buf));
	if (sendto(csd, buf, sizeof(buf), 0, (struct sockaddr *)&addr,
	    sizeof(addr)) != sizeof(buf))
		test_fail("Can't send to socket");
	if (recvfrom(sd, buf, sizeof(buf), 0, NULL, 0) != sizeof(buf))
		test_fail("Can't receive from socket");
	if (buf[0] != 'X')
		test_fail("Transmission failure");

	if (unlink(TEST_SUN_PATH) != 0)
		test_fail("Can't unlink socket");

	if ((sd2 = socket(PF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0)) == -1)
		test_fail("Can't open socket");
	if (bind(sd2, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		test_fail("Can't bind socket");
	memset(&saddr2, 0, sizeof(saddr2));
	len = sizeof(saddr2);
	if (getsockname(sd2, (struct sockaddr *)&saddr2, &len) != 0)
		test_fail("Can't get socket address");
	if (memcmp(&saddr, &saddr2, sizeof(saddr)))
		test_fail("Unexpected socket address");

	memset(buf, 'Y', sizeof(buf));
	if (sendto(csd, buf, sizeof(buf), 0, (struct sockaddr *)&addr,
	    sizeof(addr)) != sizeof(buf))
		test_fail("Can't send to socket");
	if (recvfrom(sd, buf, sizeof(buf), 0, NULL, 0) != -1)
		test_fail("Unexpectedly received from old socket");
	if (errno != EWOULDBLOCK)
		test_fail("Wrong receive failure from old socket");
	if (recvfrom(sd2, buf, sizeof(buf), 0, NULL, 0) != sizeof(buf))
		test_fail("Can't receive from new socket");
	if (buf[0] != 'Y')
		test_fail("Transmission failure");

	len = sizeof(saddr2);
	if (getsockname(sd, (struct sockaddr *)&saddr2, &len) != 0)
		test_fail("Can't get old socket address");
	if (memcmp(&saddr, &saddr2, sizeof(saddr)))
		test_fail("Unexpected old socket address");

	if (unlink(TEST_SUN_PATH) != 0)
		test_fail("Can't unlink socket");

	CLOSE(sd);
	CLOSE(sd2);
	CLOSE(csd);

	/*
	 * If the socket path identifies a file that is not a socket, bind(2)
	 * should still fail with EADDRINUSE, but connect(2) and sendto(2)
	 * should fail with ENOTSOCK (the latter is not specified by POSIX, so
	 * we follow NetBSD here).
	 */
	if ((fd = open(TEST_SUN_PATH, O_WRONLY | O_CREAT | O_EXCL, 0700)) < 0)
		test_fail("Can't create regular file");
	CLOSE(fd);

	if ((sd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1)
		test_fail("Can't open socket");
	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != -1)
		test_fail("Binding socket unexpectedly succeeded");
	if (errno != EADDRINUSE)
		test_fail("Binding socket failed with wrong error");
	if (sendto(sd, buf, sizeof(buf), 0, (struct sockaddr *)&addr,
	    sizeof(addr)) != -1)
		test_fail("Sending to socket unexpectedly succeeded");
	if (errno != ENOTSOCK)
		test_fail("Sending to socket failed with wrong error");
	CLOSE(sd);

	if ((sd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1)
		test_fail("Can't open socket");
	if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) != -1)
		test_fail("Connecting socket unexpectedly succeeded");
	if (errno != ENOTSOCK)
		test_fail("Connecting socket failed with wrong error");
	CLOSE(sd);

	UNLINK(TEST_SUN_PATH);

	/*
	 * The created socket file should have an access mode of 0777 corrected
	 * with the calling process's file mode creation mask.
	 */
	omask = umask(045123);

	if ((sd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1)
		test_fail("Can't open socket");
	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		test_fail("Can't bind socket");

	if (stat(TEST_SUN_PATH, &st) != 0)
		test_fail("Can't stat socket");
	if (!S_ISSOCK(st.st_mode))
		test_fail("File is not a socket");
	if ((st.st_mode & ALLPERMS) != (ACCESSPERMS & ~0123))
		test_fail("Unexpected file permission mask");

	CLOSE(sd);
	UNLINK(TEST_SUN_PATH);

	umask(omask);

	/*
	 * Only socket(2), socketpair(2), and accept(2) may be used to obtain
	 * new file descriptors to sockets (or "sockets"); open(2) on a socket
	 * file is expected to fail with EOPNOTSUPP (Austin Group Issue #943),
	 * regardless of whether the socket is in use.
	 */
	if ((sd = socket(PF_UNIX, SOCK_DGRAM, 0)) == -1)
		test_fail("Can't open socket");
	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
		test_fail("Can't bind socket");

	if (open(TEST_SUN_PATH, O_RDWR) != -1)
		test_fail("Unexpectedly opened socket file");
	if (errno != EOPNOTSUPP)
		test_fail("Open failed with wrong error");

	CLOSE(sd);

	if (open(TEST_SUN_PATH, O_RDONLY) != -1)
		test_fail("Unexpectedly opened socket file");
	if (errno != EOPNOTSUPP)
		test_fail("Open failed with wrong error");

	UNLINK(TEST_SUN_PATH);
}

int main(int argc, char *argv[])
{
	int i;
	struct sockaddr_un clientaddr = {
		.sun_family = AF_UNIX,
		.sun_path = TEST_SUN_PATH,
	};
	struct sockaddr_un clientaddr2 = {
		.sun_family = AF_UNIX,
		.sun_path = TEST_SUN_PATHB,
	};
	struct sockaddr_un clientaddrsym = {
		.sun_family = AF_UNIX,
		.sun_path = TEST_SYM_A,
	};
	const struct socket_test_info info = {
		.clientaddr               = (struct sockaddr *) &clientaddr,
		.clientaddrlen            = sizeof(clientaddr),
		.clientaddr2              = (struct sockaddr *) &clientaddr2,
		.clientaddr2len           = sizeof(clientaddr2),
		.clientaddrsym            = (struct sockaddr *) &clientaddrsym,
		.clientaddrsymlen         = sizeof(clientaddrsym),
		.domain                   = PF_UNIX,
		.expected_rcvbuf          = 32768 - 5, /* no constants: */
		.expected_sndbuf          = 32768 - 5, /* UDS internals */
		.serveraddr               = (struct sockaddr *) &clientaddr,
		.serveraddrlen            = sizeof(clientaddr),
		.serveraddr2              = (struct sockaddr *) &clientaddr2,
		.serveraddr2len           = sizeof(clientaddr2),
		.type                     = SOCK_STREAM,
		.types                    = types,
		.typecount                = 3,
		.callback_check_sockaddr  = callback_check_sockaddr,
		.callback_cleanup         = callback_cleanup,
		.callback_xfer_prepclient = callback_xfer_prepclient,
		.callback_xfer_peercred   = callback_xfer_peercred,
		.callback_set_listen_opt  = callback_set_listen_opt,
	};

	debug("entering main()");

	start(56);

	/* This test was written before UDS started supporting SIGPIPE. */
	signal(SIGPIPE, SIG_IGN);

	test_socket(&info);
	test_bind(&info);
	test_bind_unix();
	test_listen(&info);
	test_getsockname(&info);
	test_header();
	test_shutdown(&info);
	test_close(&info);
	test_permissions();
	test_dup(&info);
	test_dup2(&info);
	test_socketpair();
	test_shutdown(&info);
	test_read(&info);
	test_write(&info);
	test_sockopts(&info);
	test_ucred();
	test_xfer(&info);

	for (i = 0; i < 3; i++) {
		test_simple_client_server(&info, types[i]);
		if (types[i] != SOCK_DGRAM) test_vectorio(types[i]);
		if (types[i] != SOCK_DGRAM) test_msg(types[i]);
	}

	test_abort_client_server(&info, 1);
	test_abort_client_server(&info, 2);
	test_msg_dgram(&info);
	test_connect(&info);
	test_multiproc_read();
	test_multiproc_write();
	test_scm_credentials();
	test_fd_passing();
	test_select();
	test_select_close();
	test_fchmod();
	test_nonblock(&info);
	test_connect_nb(&info);
	test_intr(&info);
	test_connect_close(&info);
	test_listen_close(&info);
	test_listen_close_nb(&info);
	test_file();

	quit();

	return -1;	/* we should never get here */
}
