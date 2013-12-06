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

#define DEBUG 0

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* buffer for send/recv */
#define BUFSIZE (128)

#define ISO8601_FORMAT "%Y-%m-%dT%H:%M:%S"

/* socket types supported */
int types[3] = {SOCK_STREAM, SOCK_SEQPACKET, SOCK_DGRAM};
char sock_fullpath[PATH_MAX + 1];

void test_abort_client_server(int abort_type);
void test_abort_client(int abort_type);
void test_abort_server(pid_t pid, int abort_type);

/* timestamps for debug and error logs */
char *get_timestamp(void)
{
	struct tm *tm;
	time_t t;
	size_t len;
	char *s;

	len = sizeof(char) * 32;

	t = time(NULL);
	if (t == -1) {
		return NULL;
	}
	tm = gmtime(&t);
	if (tm == NULL) {
		return NULL;
	}

	s = (char *) malloc(len);
	if (!s) {
		perror("malloc");
		return NULL;
	}
	memset(s, '\0', len);

	strftime(s, len - 1, ISO8601_FORMAT, tm);
	return s;
}

/* macro to display information about a failed test and increment the errct */
void test_fail_fl(char *msg, char *file, int line)
{
	char *timestamp;
	timestamp = get_timestamp();
	if (errct == 0) fprintf(stderr, "\n");
	fprintf(stderr, "[ERROR][%s] (%s Line %d) %s [pid=%d:errno=%d:%s]\n",
			timestamp, file, line, msg, getpid(),
					errno, strerror(errno));
	fflush(stderr);
	if (timestamp != NULL) {
		free(timestamp);
		timestamp = NULL;
	}
	e(7);
}
#define test_fail(msg)	test_fail_fl(msg, __FILE__, __LINE__)

/* Convert name to the full path of the socket. Assumes name is in cwd. */
char *fullpath(char *name)
{
	char cwd[PATH_MAX + 1];

	if (realpath(".", cwd) == NULL)
		test_fail("Couldn't retrieve current working dir");

	snprintf(sock_fullpath, PATH_MAX, "%s/%s", cwd, name);

	return(sock_fullpath);
}

#if DEBUG == 1
/* macros to display debugging information */
void debug_fl(char *msg, char *file, int line)
{
	char *timestamp;
	timestamp = get_timestamp();
	fprintf(stdout,"[DEBUG][%s] (%s:%d) %s [pid=%d]\n",
		timestamp, __FILE__, __LINE__, msg, getpid());
	fflush(stdout);
	if (timestamp != NULL) {
		free(timestamp);
		timestamp = NULL;
	}
}
#define debug(msg) debug_fl(msg, __FILE__, __LINE__)
#else
#define debug(msg)
#endif

#define SOCKET(sd,domain,type,protocol)					\
	do {								\
		errno = 0;						\
		sd = socket(domain, type, protocol);			\
		if (sd == -1) {						\
		test_fail("sd = socket(domain, type, protocol) failed");\
		}							\
	} while (0)

#define UNLINK(path)						\
	do {							\
		int rc;						\
		errno = 0;					\
		rc = unlink(path);				\
		if (rc == -1 && errno != ENOENT) {		\
			test_fail("unlink(path) failed");	\
		}						\
	} while(0)

#define SYMLINK(oldpath,newpath)					\
	do {								\
		int rc;							\
		errno = 0;						\
		rc = symlink(oldpath,newpath);				\
		if (rc == -1) {						\
			test_fail("symlink(oldpath,newpath) failed");	\
		}							\
	} while(0)

#define CLOSE(sd)					\
	do {						\
		int rc;					\
		errno = 0;				\
		rc = close(sd);				\
		if (rc == -1) {				\
			test_fail("close(sd) failed");	\
		}					\
	} while (0)

void test_socket(void)
{
	struct stat statbuf, statbuf2;
	int sd, sd2;
	int rc;
	int i;

	debug("entering test_socket()");

	debug("Test socket() with an unsupported address family");

	errno = 0;
	sd = socket(-1, SOCK_STREAM, 0);
	if (!(sd == -1 && errno == EAFNOSUPPORT)) {
		test_fail("socket");
		if (sd != -1) {
			CLOSE(sd);
		}
	}

	debug("Test socket() with all available FDs open by this process");

	for (i = 3; i < getdtablesize(); i++) {
		rc = open("/dev/null", O_RDONLY);
		if (rc == -1) {
			test_fail("we couldn't open /dev/null for read");
		}
	}

	errno = 0;
	sd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (!(sd == -1 && errno == EMFILE)) {
		test_fail("socket() call with all fds open should fail");
		if (sd != -1) {
			CLOSE(sd);
		}
	}

	for (i = 3; i < getdtablesize(); i++) {
		CLOSE(i);
	}

	debug("Test socket() with an mismatched protocol");

	errno = 0;
	sd = socket(PF_UNIX, SOCK_STREAM, 4);
	if (!(sd == -1 && errno == EPROTONOSUPPORT)) {
		test_fail("socket() should fail with errno = EPROTONOSUPPORT");
		if (sd != -1) {
			CLOSE(sd);
		}
	}

	debug("Test socket() success");

	/*
	 * open 2 sockets at once and *then* close them.
	 * This will test that /dev/uds is cloning properly.
	 */

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	SOCKET(sd2, PF_UNIX, SOCK_STREAM, 0);

	rc = fstat(sd, &statbuf);
	if (rc == -1) {
		test_fail("fstat failed on sd");
	}

	rc = fstat(sd2, &statbuf2);
	if (rc == -1) {
		test_fail("fstat failed on sd2");
	}


	if (statbuf.st_dev == statbuf2.st_dev) {
		test_fail("/dev/uds isn't being cloned");
	}

	CLOSE(sd2);
	CLOSE(sd);

	debug("leaving test_socket()");
}

void test_header(void)
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

void test_socketpair(void)
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

void test_ucred(void)
{
	struct uucred credentials;
	socklen_t ucred_length;
	uid_t euid = geteuid();
	gid_t egid = getegid();
	int sv[2];
	int rc;

	debug("Test credentials passing");

	ucred_length = sizeof(struct uucred);

	rc = socketpair(PF_UNIX, SOCK_STREAM, 0, sv);
	if (rc == -1) {
		test_fail("socketpair(PF_UNIX, SOCK_STREAM, 0, sv) failed");
	}

	memset(&credentials, '\0', ucred_length);
	rc = getsockopt(sv[0], SOL_SOCKET, SO_PEERCRED, &credentials, 
							&ucred_length);
	if (rc == -1) {
		test_fail("getsockopt(SO_PEERCRED) failed");
	} else if (credentials.cr_ngroups != 0 ||
			credentials.cr_uid != geteuid() ||
			credentials.cr_gid != getegid()) {
		/* printf("%d=%d %d=%d %d=%d",credentials.cr_ngroups, 0,
		 credentials.cr_uid, geteuid(), credentials.cr_gid, getegid()); */
		test_fail("Credential passing gave us the wrong cred");
	}

	rc = getpeereid(sv[0], &euid, &egid);
	if (rc == -1) {
		test_fail("getpeereid(sv[0], &euid, &egid) failed");
	} else if (credentials.cr_uid != euid || credentials.cr_gid != egid) {
		test_fail("getpeereid() didn't give the correct euid/egid");
	}

	CLOSE(sv[0]);
	CLOSE(sv[1]);
}

void test_getsockname(void)
{
	int sd;
	int rc;
	struct sockaddr_un addr, sock_addr;
	socklen_t sock_addr_len;

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc == -1) {
		test_fail("bind() should have worked");
	}

	debug("Test getsockname() success");

	memset(&sock_addr, '\0', sizeof(struct sockaddr_un));
	sock_addr_len = sizeof(struct sockaddr_un);

	rc = getsockname(sd, (struct sockaddr *) &sock_addr, &sock_addr_len);
	if (rc == -1) {
		test_fail("getsockname() should have worked");
	}

	if (!(sock_addr.sun_family == AF_UNIX && strncmp(sock_addr.sun_path,
		fullpath(TEST_SUN_PATH),
		sizeof(sock_addr.sun_path) - 1) == 0)) {
		test_fail("getsockname() did return the right address");
		fprintf(stderr, "exp: '%s' | got: '%s'\n", addr.sun_path,
							sock_addr.sun_path);
	}

	CLOSE(sd);
}

void test_bind(void)
{
	struct sockaddr_un addr;
	struct sockaddr_un sock_addr;
	socklen_t sock_addr_len;
	int sd;
	int sd2;
	int rc;

	debug("entering test_bind()");
	UNLINK(TEST_SUN_PATH);
	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);

	debug("Test bind() success");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc == -1) {
		test_fail("bind() should have worked");
	}

	debug("Test getsockname() success");

	memset(&sock_addr, '\0', sizeof(struct sockaddr_un));
	sock_addr_len = sizeof(struct sockaddr_un);

	rc = getsockname(sd, (struct sockaddr *) &sock_addr, &sock_addr_len);
	if (rc == -1) {
		test_fail("getsockname() should have worked");
	}

	if (!(sock_addr.sun_family == AF_UNIX &&
			strncmp(sock_addr.sun_path, 
			fullpath(TEST_SUN_PATH),
			sizeof(sock_addr.sun_path) - 1) == 0)) {

		test_fail("getsockname() didn't return the right addr");
		fprintf(stderr, "exp: '%s' | got: '%s'\n", addr.sun_path,
							sock_addr.sun_path);
	}

	debug("Test bind() with a address that has already been bind()'d");

	SOCKET(sd2, PF_UNIX, SOCK_STREAM, 0);
	errno = 0;
	rc = bind(sd2, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (!((rc == -1) && (errno == EADDRINUSE))) {
		test_fail("bind() should have failed with EADDRINUSE");
	}
	CLOSE(sd2);
	CLOSE(sd);
	UNLINK(TEST_SUN_PATH);

	debug("Test bind() with an empty sun_path");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	memset(addr.sun_path, '\0', sizeof(addr.sun_path));
	errno = 0;

	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (!(rc == -1 && errno == ENOENT)) {
		test_fail("bind() should have failed with ENOENT");
	}
	CLOSE(sd);

	debug("Test bind() with a NULL address");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	errno = 0;
	rc = bind(sd, (struct sockaddr *) NULL, sizeof(struct sockaddr_un));
	if (!((rc == -1) && (errno == EFAULT))) {
		test_fail("bind() should have failed with EFAULT");
	}
	CLOSE(sd);

	debug("Test bind() using a symlink loop");

	UNLINK(TEST_SUN_PATH);
	UNLINK(TEST_SYM_A);
	UNLINK(TEST_SYM_B);

	SYMLINK(TEST_SYM_A, TEST_SYM_B);
	SYMLINK(TEST_SYM_B, TEST_SYM_A);

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	strncpy(addr.sun_path, TEST_SYM_A, sizeof(addr.sun_path) - 1);
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
	rc = bind(sd, (struct sockaddr *) &addr, strlen(addr.sun_path) + 1);
	if (rc == -1) {
		test_fail("bind() should have worked");
	}
	CLOSE(sd);
	UNLINK("foo");

	debug("leaving test_bind()");
}

void test_listen(void)
{
	int rc;

	debug("entering test_listen()");

	debug("Test listen() with a bad file descriptor");

	errno = 0;
	rc = listen(-1, 0);
	if (!(rc == -1 && errno == EBADF)) {
		test_fail("listen(-1, 0) should have failed");
	}

	debug("Test listen() with a non-socket file descriptor");

	errno = 0;
	rc = listen(0, 0);
	/* Test on errno disabled here: there's currently no telling what this
	 * will return. POSIX says it should be ENOTSOCK, MINIX3 libc returns
	 * ENOSYS, and we used to test for ENOTTY here..
	 */
	if (!(rc == -1)) {
		test_fail("listen(0, 0) should have failed");
	}

	debug("leaving test_listen()");
}

void test_shutdown(void)
{
	int how[3] = { SHUT_RD, SHUT_WR, SHUT_RDWR };
	int sd;
	int rc;
	int i;

	debug("entering test_shutdown()");

	/* test for each direction (read, write, read-write) */
	for (i = 0; i < 3; i++) {

		debug("test shutdown() with an invalid descriptor");

		errno = 0;
		rc = shutdown(-1, how[i]);
		if (!(rc == -1 && errno == EBADF)) {
			test_fail("shutdown(-1, how[i]) should have failed");
		}

		debug("test shutdown() with a non-socket descriptor");

		errno = 0;
		rc = shutdown(0, how[i]);
		if (!(rc == -1 && errno == ENOSYS)) {
			test_fail("shutdown() should have failed with ENOSYS");
		}

		debug("test shutdown() with a socket that is not connected");

		SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
		errno = 0;
		rc = shutdown(sd, how[i]);
		if (!(rc == -1 && errno == ENOTCONN)) {
			test_fail("shutdown() should have failed");
		}
		CLOSE(sd);
	}

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	errno = 0;
	rc = shutdown(sd, -1);
	if (!(rc == -1 && errno == ENOTCONN)) {
		test_fail("shutdown(sd, -1) should have failed with ENOTCONN");
	}
	CLOSE(sd);

	debug("leaving test_shutdown()");
}

void test_close(void)
{
	struct sockaddr_un addr;
	int sd, sd2;
	int rc, i;

	debug("entering test_close()");

	UNLINK(TEST_SUN_PATH);

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	debug("Test close() success");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc != 0) {
		test_fail("bind() should have worked");
	}

	CLOSE(sd);

	debug("Close an already closed file descriptor");

	errno = 0;
	rc = close(sd);
	if (!(rc == -1 && errno == EBADF)) {
		test_fail("close(sd) should have failed with EBADF");
	}

	UNLINK(TEST_SUN_PATH);

	debug("dup()'ing a file descriptor and closing both should work");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc != 0) {
		test_fail("bind() should have worked");
	}

	errno = 0;
	sd2 = dup(sd);
	if (sd2 == -1) {
		test_fail("dup(sd) should have worked");
	} else {
		CLOSE(sd2);
		CLOSE(sd);
	}

	UNLINK(TEST_SUN_PATH);

	/* Create and close a socket a bunch of times.
	 * If the implementation doesn't properly free the
	 * socket during close(), eventually socket() will
	 * fail when the internal descriptor table is full.
	 */
	for (i = 0; i < 1024; i++) {
		SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
		CLOSE(sd);
	}

	debug("leaving test_close()");
}

void test_sockopts(void)
{
	int i;
	int rc;
	int sd;
	int option_value;
	socklen_t option_len;

	debug("entering test_sockopts()");

	for (i = 0; i < 3; i++) {

		SOCKET(sd, PF_UNIX, types[i], 0);

		debug("Test setsockopt() works");

		option_value = 0;
		option_len = sizeof(option_value);
		errno = 0;
		rc = getsockopt(sd, SOL_SOCKET, SO_TYPE, &option_value, 
							&option_len);
		if (rc != 0) {
			test_fail("setsockopt() should have worked");
		}

		if (option_value != types[i]) {
			test_fail("SO_TYPE didn't seem to work.");
		}

		CLOSE(sd);
	}



	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	debug("Test setsockopt() works");

	option_value = 0;
	option_len = sizeof(option_value);
	errno = 0;
	rc = getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &option_value, &option_len);
	if (rc != 0) {
		test_fail("getsockopt() should have worked");
	}

	if (option_value != PIPE_BUF) {
		test_fail("SO_SNDBUF didn't seem to work.");
	}

	CLOSE(sd);


	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	debug("Test setsockopt() works");

	option_value = 0;
	option_len = sizeof(option_value);
	errno = 0;
	rc = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &option_value, &option_len);
	if (rc != 0) {
		test_fail("getsockopt() should have worked");
	}

	if (option_value != PIPE_BUF) {
		test_fail("SO_RCVBUF didn't seem to work.");
	}

	CLOSE(sd);


	debug("leaving test_sockopts()");
}

void test_read(void)
{
	int rc;
	int fd;
	char buf[BUFSIZE];

	debug("entering test_read()");

	errno = 0;
	rc = read(-1, buf, sizeof(buf));
	if (!(rc == -1 && errno == EBADF)) {
		test_fail("read() should have failed with EBADF");
	}

	fd = open("/tmp", O_RDONLY);
	if (fd == -1) {
		test_fail("open(\"/tmp\", O_RDONLY) should have worked");
	}

	CLOSE(fd);

	debug("leaving test_read()");
}

void test_write(void)
{
	int rc;
	char buf[BUFSIZE];

	debug("entering test_write()");

	errno = 0;
	rc = write(-1, buf, sizeof(buf));
	if (!(rc == -1 && errno == EBADF)) {
		test_fail("write() should have failed with EBADF");
	}

	debug("leaving test_write()");
}

void test_dup(void)
{
	struct stat info1;
	struct stat info2;
	struct sockaddr_un addr;
	int sd, sd2;
	int rc;
	int i;

	debug("entering test_dup()");

	UNLINK(TEST_SUN_PATH);

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	debug("Test dup()");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);
	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc != 0) {
		test_fail("bind() should have worked");
	}

	errno = 0;
	sd2 = dup(sd);
	if (sd2 == -1) {
		test_fail("dup(sd) should have worked");
	}

	rc = fstat(sd, &info1);
	if (rc == -1) {
		test_fail("fstat(fd, &info1) failed");
	}

	rc = fstat(sd2, &info2);
	if (rc == -1) {
		test_fail("fstat(sd, &info2) failed");
	}

	if (info1.st_ino != info2.st_ino) {
		test_fail("dup() failed info1.st_ino != info2.st_ino");
	}

	CLOSE(sd);
	CLOSE(sd2);

	debug("Test dup() with a closed socket");

	errno = 0;
	rc = dup(sd);
	if (!(rc == -1 && errno == EBADF)) {
		test_fail("dup(sd) on a closed socket shouldn't have worked");
	}

	debug("Test dup() with socket descriptor of -1");

	errno = 0;
	rc = dup(-1);
	if (!(rc == -1 && errno == EBADF)) {
		test_fail("dup(-1) shouldn't have worked");
	}

	debug("Test dup() when all of the file descriptors are taken");

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	for (i = 4; i < getdtablesize(); i++) {
		rc = open("/dev/null", O_RDONLY);
		if (rc == -1) {
			test_fail("we couldn't open /dev/null for read");
		}
	}

	errno = 0;
	sd2 = dup(sd);
	if (!(sd2 == -1 && errno == EMFILE)) {
		test_fail("dup(sd) should have failed with errno = EMFILE");
	}

	for (i = 3; i < getdtablesize(); i++) {
		CLOSE(i);
	}

	UNLINK(TEST_SUN_PATH);

	debug("leaving test_dup()");
}

void test_dup2(void)
{
	struct stat info1;
	struct stat info2;
	struct sockaddr_un addr;
	int sd;
	int fd;
	int rc;

	debug("entering test_dup2()");
	UNLINK(TEST_SUN_PATH);

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc != 0) {
		test_fail("bind() should have worked");
	}

	fd = open("/dev/null", O_RDONLY);
	if (fd == -1) {
		test_fail("open(\"/dev/null\", O_RDONLY) failed");
	}

	fd = dup2(sd, fd);
	if (fd == -1) {
		test_fail("dup2(sd, fd) failed.");
	}

	memset(&info1, '\0', sizeof(struct stat));
	memset(&info2, '\0', sizeof(struct stat));

	rc = fstat(fd, &info1);
	if (rc == -1) {
		test_fail("fstat(fd, &info1) failed");
	}

	rc = fstat(sd, &info2);
	if (rc == -1) {
		test_fail("fstat(sd, &info2) failed");
	}

	if (!(info1.st_ino == info2.st_ino &&
		major(info1.st_dev) == major(info2.st_dev) && 
		minor(info1.st_dev) == minor(info2.st_dev))) {

		test_fail("dup2() failed");
	}

	CLOSE(fd);
	CLOSE(sd);

	UNLINK(TEST_SUN_PATH);
	debug("leaving test_dup2()");

}

/*
 * A toupper() server. This toy server converts a string to upper case.
 */
void test_xfer_server(pid_t pid)
{
	int i;
	struct timeval tv;
	fd_set readfds;
	int status;
	int rc;
	int sd;
	char buf[BUFSIZE];
	socklen_t client_addr_size;
	int client_sd;
	struct sockaddr_un addr;
	struct sockaddr_un client_addr;

	status = 0;
	rc = 0;
	sd = 0;
	client_sd = 0;
	client_addr_size = sizeof(struct sockaddr_un);

	memset(&buf, '\0', sizeof(buf));
	memset(&addr, '\0', sizeof(struct sockaddr_un));
	memset(&client_addr, '\0', sizeof(struct sockaddr_un));

	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc == -1) {
		test_fail("bind() should have worked");
	}

	rc = listen(sd, 8);
	if (rc == -1) {
		test_fail("listen(sd, 8) should have worked");
	}

	/* we're ready for connections, time to tell the client to start
	 * the test
	 */
	kill(pid, SIGUSR1);

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	FD_ZERO(&readfds);
	FD_SET(sd, &readfds);

	/* use select() in case the client is really broken and never 
	 * attempts to connect (we don't want to block on accept() 
	 * forever).
	 */
	rc = select(sd + 1, &readfds, NULL, NULL, &tv);
	if (rc == -1) {
		test_fail("[server] select() should not have failed");
	}

	if (rc != 1) {
		test_fail("[server] select() should have returned 1");
		printf("[server] select returned %d\n", rc);
	}

	if (!(FD_ISSET(sd, &readfds))) {
		test_fail("[server] client didn't connect within 10 seconds");
		kill(pid, SIGKILL);
		return;
	}

	client_sd = accept(sd, (struct sockaddr *) &client_addr,
						&client_addr_size);

	if (client_sd == -1) {
		test_fail("accept() should have worked");
		kill(pid, SIGKILL);
		return;
	} else {
		debug("[server] client accept()'d");
	}

	debug("[server] Reading message");
	rc = read(client_sd, buf, sizeof(buf));
	if (rc == -1) {
		test_fail("read() failed unexpectedly");
		kill(pid, SIGKILL);
		return;
	}
	debug("[server] we got the following message:");
	debug(buf);

	for (i = 0; i < rc && i < 127; i++) {
		buf[i] = toupper(buf[i]);
	}

	debug("[server] Writing message...");
	rc = write(client_sd, buf, sizeof(buf));
	if (rc == -1) {
		test_fail("write(client_sd, buf, sizeof(buf)) failed");
		kill(pid, SIGKILL);
		return;
	}

	if (rc < strlen(buf)) {
		test_fail("[server] write didn't write all the bytes");
	}

	memset(&buf, '\0', sizeof(buf));

	debug("[server] Recv message");
	rc = recv(client_sd, buf, sizeof(buf), 0);
	if (rc == -1) {
		test_fail("recv() failed unexpectedly");
		kill(pid, SIGKILL);
		return;
	}
	debug("[server] we got the following message:");
	debug(buf);

	for (i = 0; i < rc && i < 127; i++) {
		buf[i] = toupper(buf[i]);
	}

	debug("[server] Sending message...");
	rc = send(client_sd, buf, sizeof(buf), 0);
	if (rc == -1) {
		test_fail("send(client_sd, buf, sizeof(buf), 0) failed");
		kill(pid, SIGKILL);
		return;
	}

	if (rc < strlen(buf)) {
		test_fail("[server] write didn't write all the bytes");
	}

	memset(&buf, '\0', sizeof(buf));

	debug("[server] Recvfrom message");
	rc = recvfrom(client_sd, buf, sizeof(buf), 0, NULL, 0);
	if (rc == -1) {
		test_fail("recvfrom() failed unexpectedly");
		kill(pid, SIGKILL);
		return;
	}
	debug("[server] we got the following message:");
	debug(buf);

	for (i = 0; i < rc && i < 127; i++) {
		buf[i] = toupper(buf[i]);
	}

	debug("[server] Sendto message...");
	rc = sendto(client_sd, buf, sizeof(buf), 0, NULL, 0);
	if (rc == -1) {
		test_fail("sendto() failed");
		kill(pid, SIGKILL);
		return;
	}

	if (rc < strlen(buf)) {
		test_fail("[server] write didn't write all the bytes");
	}

	shutdown(client_sd, SHUT_RDWR);
	CLOSE(client_sd);

	shutdown(sd, SHUT_RDWR);
	CLOSE(sd);

	/* wait for client to exit */
	do {
		errno = 0;
		rc = waitpid(pid, &status, 0);
	} while (rc == -1 && errno == EINTR);

	/* we use the exit status to get its error count */
	errct += WEXITSTATUS(status);
}

int server_ready = 0;

/* signal handler for the client */
void test_xfer_sighdlr(int sig)
{
	debug("entering signal handler");
	switch (sig) {
		/* the server will send SIGUSR1 when it is time for us 
		 * to start the tests
		 */
	case SIGUSR1:
		server_ready = 1;
		debug("got SIGUSR1, the server is ready for the client");
		break;
	default:
		debug("didn't get SIGUSR1");
	}
	debug("leaving signal handler");
}

/*
 * A toupper() client.
 */
void test_xfer_client(void)
{
	struct uucred credentials;
	socklen_t ucred_length;
	struct timeval tv;
	fd_set readfds;
	struct sockaddr_un addr;
	struct sockaddr_un peer_addr;
	socklen_t peer_addr_len;
	int sd;
	int rc;
	char buf[BUFSIZE];

	debug("[client] entering test_xfer_client()");
	errct = 0;	/* reset error count */
	ucred_length = sizeof(struct uucred);
	memset(&buf, '\0', sizeof(buf));

	while (server_ready == 0) {
		debug("[client] waiting for the server to signal");
		sleep(1);
	}

	peer_addr_len = sizeof(struct sockaddr_un);


	debug("Creating symlink to TEST_SUN_PATH");

	SYMLINK(TEST_SUN_PATH, TEST_SYM_A);

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	strncpy(addr.sun_path, TEST_SYM_A, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	debug("[client] creating client socket");
	SOCKET(sd, PF_UNIX, SOCK_STREAM, 0);

	debug("[client] connecting to server through the symlink");
	rc = connect(sd, (struct sockaddr *) &addr,
						sizeof(struct sockaddr_un));
	if (rc == -1) {
		test_fail("[client] connect() should have worked");
	} else {
		debug("[client] connected");
	}

	debug("[client] testing getpeername()");
	memset(&peer_addr, '\0', sizeof(struct sockaddr_un));
	rc = getpeername(sd, (struct sockaddr *) &peer_addr, &peer_addr_len);
	if (rc == -1) {
		test_fail("[client] getpeername() should have worked");
	}

	/* we need to use the full path "/usr/src/test/DIR_56/test.sock"
	 * because that is what is returned by getpeername().
	 */

	if (!(peer_addr.sun_family == AF_UNIX &&
			strncmp(peer_addr.sun_path,
			fullpath(TEST_SUN_PATH),
			sizeof(peer_addr.sun_path) - 1) == 0)) {

		test_fail("getpeername() didn't return the right address");
	}

	strncpy(buf, "Hello, World!", sizeof(buf) - 1);
	debug("[client] send to server");
	rc = write(sd, buf, sizeof(buf));
	if (rc == -1) {
		test_fail("[client] write() failed unexpectedly");
	}

	memset(buf, '\0', sizeof(buf));
	debug("[client] read from server");
	rc = read(sd, buf, sizeof(buf));
	if (rc == -1) {
		test_fail("[client] read() failed unexpectedly");
	} else {
		debug("[client] we got the following message:");
		debug(buf);
	}

	if (strncmp(buf, "HELLO, WORLD!", sizeof(buf)) != 0) {
		test_fail("[client] We didn't get the correct response");
	}

	memset(&buf, '\0', sizeof(buf));
	strncpy(buf, "Bonjour!", sizeof(buf) - 1);

	debug("[client] send to server");
	rc = send(sd, buf, sizeof(buf), 0);
	if (rc == -1) {
		test_fail("[client] send() failed unexpectedly");
	}

	debug("Test passing the client credentials to the server");

	memset(&credentials, '\0', ucred_length);
	rc = getsockopt(sd, SOL_SOCKET, SO_PEERCRED, &credentials,
							&ucred_length);

	if (rc == -1) {
		test_fail("[client] getsockopt() failed");
	}  else if (credentials.cr_uid != geteuid() ||
					credentials.cr_gid != getegid()) {
		printf("%d=%d=%d %d=%d=%d\n", credentials.cr_uid, getuid(),
			geteuid(), credentials.cr_gid, getgid(), getegid());
		test_fail("[client] Credential passing gave us a bad UID/GID");
	}

	debug("Testing select()");

	tv.tv_sec = 2;
	tv.tv_usec = 500000;

	FD_ZERO(&readfds);
	FD_SET(sd, &readfds);

	rc = select(sd + 1, &readfds, NULL, NULL, &tv);
	if (rc == -1) {
		test_fail("[client] select() should not have failed");
	}

	if (rc != 1) {
		test_fail("[client] select() should have returned 1");
	}

	if (!(FD_ISSET(sd, &readfds))) {
		test_fail("The server didn't respond within 2.5 seconds");
	}

	memset(buf, '\0', sizeof(buf));
	debug("[client] recv from server");
	rc = recv(sd, buf, sizeof(buf), 0);
	if (rc == -1) {
		test_fail("[client] recv() failed unexpectedly");
	} else {
		debug("[client] we got the following message:");
		debug(buf);
	}

	if (strncmp(buf, "BONJOUR!", sizeof(buf)) != 0) {
		test_fail("[client] We didn't get the right response.");
	}

	memset(&buf, '\0', sizeof(buf));
	strncpy(buf, "Hola!", sizeof(buf) - 1);

	debug("[client] sendto to server");
	rc = sendto(sd, buf, sizeof(buf), 0, NULL, 0);
	if (rc == -1) {
		test_fail("[client] sendto() failed");
	}

	debug("Testing select()");

	tv.tv_sec = 2;
	tv.tv_usec = 500000;

	FD_ZERO(&readfds);
	FD_SET(sd, &readfds);

	rc = select(sd + 1, &readfds, NULL, NULL, &tv);
	if (rc == -1) {
		test_fail("[client] select() should not have failed");
	}

	if (rc != 1) {
		test_fail("[client] select() should have returned 1");
	}

	if (!(FD_ISSET(sd, &readfds))) {
		test_fail("[client] The server didn't respond in 2.5 seconds");
	}

	memset(buf, '\0', sizeof(buf));
	debug("[client] recvfrom from server");
	rc = recvfrom(sd, buf, sizeof(buf), 0, NULL, 0);
	if (rc == -1) {
		test_fail("[cleint] recvfrom() failed unexpectedly");
	} else {
		debug("[client] we got the following message:");
		debug(buf);
	}

	if (strncmp(buf, "HOLA!", sizeof(buf)) != 0) {
		test_fail("[client] We didn't get the right response.");
	}

	debug("[client] closing socket");
	CLOSE(sd);

	debug("[client] leaving test_xfer_client()");
	exit(errct);
}

void test_xfer(void)
{
	pid_t pid;

	UNLINK(TEST_SYM_A);
	UNLINK(TEST_SUN_PATH);

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
	if (pid == -1) {
		test_fail("fork() failed");
		return;
	} else if (pid == 0) {
		debug("child");
		test_xfer_client();
		test_fail("we should never get here");
		exit(1);
	} else {
		debug("parent");
		test_xfer_server(pid);
		debug("parent done");
	}

	UNLINK(TEST_SYM_A);
	UNLINK(TEST_SUN_PATH);
}

void test_simple_client(int type)
{
	char buf[BUFSIZE];
	int sd, rc;
	struct sockaddr_un addr;

	sd = socket(PF_UNIX, type, 0);
	if (sd == -1) {
		test_fail("socket");
		exit(errct);
	}

	while (server_ready == 0) {
		debug("[client] waiting for the server");
		sleep(1);
	}

	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	bzero(buf, BUFSIZE);
	snprintf(buf, BUFSIZE-1, "Hello, My Name is Client.");

	if (type == SOCK_DGRAM) {

		rc = sendto(sd, buf, strlen(buf) + 1, 0,
			(struct sockaddr *) &addr, sizeof(struct sockaddr_un));
		if (rc == -1) {
			test_fail("sendto");
			exit(errct);
		}

	} else {

		rc = connect(sd, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_un));
		if (rc == -1) {
			test_fail("connect");
			exit(errct);
		}

		rc = write(sd, buf, strlen(buf) + 1);

		if (rc == -1) {
			test_fail("write");
		}

		memset(buf, '\0', BUFSIZE);
		rc = read(sd, buf, BUFSIZE);
		if (rc == -1) {
			test_fail("read");
		}

		if (strcmp("Hello, My Name is Server.", buf) != 0) {
			test_fail("didn't read the correct string");
		}
	}

	rc = close(sd);
	if (rc == -1) {
		test_fail("close");
	}

	exit(errct);
}

void test_simple_server(int type, pid_t pid)
{
	char buf[BUFSIZE];
	int sd, rc, client_sd, status;
	struct sockaddr_un addr;
	socklen_t addr_len;

	addr_len = sizeof(struct sockaddr_un);

	sd = socket(PF_UNIX, type, 0);
	if (sd == -1) {
		test_fail("socket");
	}

	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc == -1) {
		test_fail("bind");
	}

	if (type == SOCK_DGRAM) {

		/* ready for client */
		kill(pid, SIGUSR1);

		rc = recvfrom(sd, buf, BUFSIZE, 0,
				(struct sockaddr *) &addr, &addr_len);
		if (rc == -1) {
			test_fail("recvfrom");
		}

	} else {

		rc = listen(sd, 5);
		if (rc == -1) {
			test_fail("listen");
		}

		/* we're ready for connections, time to tell the client 
		 * to start the test
		 */
		kill(pid, SIGUSR1);

		client_sd = accept(sd, (struct sockaddr *) &addr, &addr_len);
		if (client_sd == -1) {
			test_fail("accept");
		}

		memset(buf, '\0', BUFSIZE);
		rc = read(client_sd, buf, BUFSIZE);
		if (rc == -1) {
			test_fail("read");
		}

		if (strcmp("Hello, My Name is Client.", buf) != 0) {
			test_fail("didn't read the correct string");
		}

		/* added for extra fun to make the client block on read() */
		sleep(1);

		bzero(buf, BUFSIZE);
		snprintf(buf, BUFSIZE-1, "Hello, My Name is Server.");

		rc = write(client_sd, buf, strlen(buf) + 1);
		if (rc == -1) {
			test_fail("write");
		}
		rc = close(client_sd);
		if (rc == -1) {
			test_fail("close");
		}
	}

	rc = close(sd);
	if (rc == -1) {
		test_fail("close");
	}

	/* wait for client to exit */
	do {
		errno = 0;
		rc = waitpid(pid, &status, 0);
	} while (rc == -1 && errno == EINTR);

	/* we use the exit status to get its error count */
	errct += WEXITSTATUS(status);
}

void test_abort_client_server(int abort_type)
{
	pid_t pid;
	debug("test_simple_client_server()");

	UNLINK(TEST_SUN_PATH);

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
	if (pid == -1) {
		test_fail("fork() failed");
		return;
	} else if (pid == 0) {
		debug("child");
		test_abort_client(abort_type);
		test_fail("we should never get here");
		exit(1);
	} else {
		debug("parent");
		test_abort_server(pid, abort_type);
		debug("parent done");
	}

	UNLINK(TEST_SUN_PATH);
}

void test_abort_client(int abort_type)
{
	char buf[BUFSIZE];
	int sd, rc;
	struct sockaddr_un addr;

	sd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		test_fail("socket");
		exit(errct);
	}

	while (server_ready == 0) {
		debug("[client] waiting for the server");
		sleep(1);
	}

	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	bzero(buf, BUFSIZE);
	snprintf(buf, BUFSIZE-1, "Hello, My Name is Client.");

	rc = connect(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc == -1) {
		test_fail("connect");
		exit(errct);
	}

	if (abort_type == 2) {
		/* Give server a chance to close connection */
		sleep(2);
		rc = write(sd, buf, strlen(buf) + 1);
		if (rc != -1) {
			test_fail("write should have failed\n");
		}
		if (errno != ECONNRESET) {
			test_fail("errno should've been ECONNRESET\n");
		}
	}

	rc = close(sd);
	if (rc == -1) {
		test_fail("close");
	}

	exit(errct);
}

void test_abort_server(pid_t pid, int abort_type)
{
	char buf[BUFSIZE];
	int sd, rc, client_sd, status;
	struct sockaddr_un addr;
	socklen_t addr_len;

	addr_len = sizeof(struct sockaddr_un);

	sd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		test_fail("socket");
	}

	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);
	addr.sun_family = AF_UNIX;

	rc = bind(sd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));
	if (rc == -1) {
		test_fail("bind");
	}

	rc = listen(sd, 5);
	if (rc == -1) {
		test_fail("listen");
	}

	/* we're ready for connections, time to tell the client 
	 * to start the test
	 */
	kill(pid, SIGUSR1);

	client_sd = accept(sd, (struct sockaddr *) &addr, &addr_len);
	if (client_sd == -1) {
		test_fail("accept");
	}

	if (abort_type == 1) {
		memset(buf, '\0', BUFSIZE);
		rc = read(client_sd, buf, BUFSIZE);
		if (rc != -1) {
			test_fail("read should've failed\n");
		}
		if (errno != ECONNRESET) {
			test_fail("errno should've been ECONNRESET\n");
		}
	} /* else if (abort_type == 2) { */
		rc = close(client_sd);
		if (rc == -1) {
			test_fail("close");
		}
	/* } */

	rc = close(sd);
	if (rc == -1) {
		test_fail("close");
	}

	/* wait for client to exit */
	do {
		errno = 0;
		rc = waitpid(pid, &status, 0);
	} while (rc == -1 && errno == EINTR);

	/* we use the exit status to get its error count */
	errct += WEXITSTATUS(status);
}

void test_simple_client_server(int type)
{
	pid_t pid;
	debug("test_simple_client_server()");

	UNLINK(TEST_SUN_PATH);

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
	if (pid == -1) {
		test_fail("fork() failed");
		return;
	} else if (pid == 0) {
		debug("child");
		test_simple_client(type);
		test_fail("we should never get here");
		exit(1);
	} else {
		debug("parent");
		test_simple_server(type, pid);
		debug("parent done");
	}

	UNLINK(TEST_SUN_PATH);
}

void test_vectorio(int type)
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

void test_msg(int type)
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

void test_msg_dgram(void)
{
	int rc;
	int src;
	int dst;
	struct sockaddr_un addr;
	struct iovec iov[3];
	struct msghdr msg1;
	struct msghdr msg2;
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	char buf3[BUFSIZE];
	socklen_t addrlen = sizeof(struct sockaddr_un);

	debug("test msg_dgram");

	UNLINK(TEST_SUN_PATH);
	UNLINK(TEST_SUN_PATHB);

	src = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (src == -1) {
		test_fail("socket");
	}

	dst = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (dst == -1) {
		test_fail("socket");
	}

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATHB, sizeof(addr.sun_path) - 1);
	rc = bind(src, (struct sockaddr *) &addr, addrlen);
	if (rc == -1) {
		test_fail("bind");
	}

	memset(&addr, '\0', sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, TEST_SUN_PATH, sizeof(addr.sun_path) - 1);

	rc = bind(dst, (struct sockaddr *) &addr, addrlen);
	if (rc == -1) {
		test_fail("bind");
	}

	memset(&buf1, '\0', BUFSIZE);
	memset(&buf2, '\0', BUFSIZE);
	memset(&buf3, '\0', BUFSIZE);

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
	msg2.msg_control = NULL;
	msg2.msg_controllen = 0;
	msg2.msg_flags = 0;

	rc = recvmsg(dst, &msg2, 0);
	if (rc == -1) {
		test_fail("recvmsg");
	}

	if (strncmp(buf1, "Minix is ", 9) || strncmp(buf2, "great!", 6)) {
		test_fail("recvmsg");
	}

	/* we need to use the full path "/usr/src/test/DIR_56/testb.sock"
	 * because that is what is returned by recvmsg().
	 */
	if (addr.sun_family != AF_UNIX || strcmp(addr.sun_path,
					fullpath(TEST_SUN_PATHB))) {
		test_fail("recvmsg");
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

void test_scm_credentials(void)
{
	int rc;
	int src;
	int dst;
	struct uucred cred;
	struct cmsghdr *cmsg = NULL;
	struct sockaddr_un addr;
	struct iovec iov[3];
	struct msghdr msg1;
	struct msghdr msg2;
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	char buf3[BUFSIZE];
	char ctrl[BUFSIZE];
	socklen_t addrlen = sizeof(struct sockaddr_un);

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

	debug("sending msg1");

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
					fullpath(TEST_SUN_PATHB))) {
		test_fail("recvmsg");
	}

	debug("looking for credentials");

	memset(&cred, '\0', sizeof(struct uucred));
	for (cmsg = CMSG_FIRSTHDR(&msg2); cmsg != NULL;
					cmsg = CMSG_NXTHDR(&msg2, cmsg)) {

		if (cmsg->cmsg_level == SOL_SOCKET &&
				cmsg->cmsg_type == SCM_CREDS) {

			memcpy(&cred, CMSG_DATA(cmsg), sizeof(struct uucred));
			break;
		}
	}

	if (cred.cr_ngroups != 0 || cred.cr_uid != geteuid() ||
						cred.cr_gid != getegid()) {

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

void test_connect(void)
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
		test_simple_client_server(types[i]);
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

int test_multiproc_read(void)
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

int test_multiproc_write(void)
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

void test_fd_passing_child(int sd)
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

void test_fd_passing_parent(int sd)
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

void test_permissions(void) {
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

void test_fd_passing(void) {
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

void test_select()
{
	int i, nfds = -1;
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

void test_select_close(void)
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

void test_fchmod()
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

static void
check_select(int sd, int rd, int wr, int block)
{
	fd_set read_set, write_set;
	struct timeval tv;

	FD_ZERO(&read_set);
	if (rd != -1)
		FD_SET(sd, &read_set);

	FD_ZERO(&write_set);
	if (wr != -1)
		FD_SET(sd, &write_set);

	tv.tv_sec = block ? 2 : 0;
	tv.tv_usec = 0;

	if (select(sd + 1, &read_set, &write_set, NULL, &tv) < 0)
		test_fail("select() failed unexpectedly");

	if (rd != -1 && !!FD_ISSET(sd, &read_set) != rd)
		test_fail("select() mismatch on read operation");

	if (wr != -1 && !!FD_ISSET(sd, &write_set) != wr)
		test_fail("select() mismatch on write operation");
}

/*
 * Verify that:
 * - a nonblocking connecting socket for which there is no accepter, will
 *   return EINPROGRESS and complete in the background later;
 * - a nonblocking listening socket will return EAGAIN on accept;
 * - connecting a connecting socket yields EALREADY;
 * - connecting a connected socket yields EISCONN;
 * - selecting for read and write on a connecting socket will only satisfy the
 *   write only once it is connected;
 * - doing a nonblocking write on a connecting socket yields EAGAIN;
 * - doing a nonblocking read on a connected socket with no pending data yields
 *   EAGAIN.
 */
static void
test_nonblock(void)
{
	char buf[BUFSIZE];
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_un server_addr, client_addr, addr;
	int status;

	memset(buf, 0, sizeof(buf));

	memset(&server_addr, 0, sizeof(server_addr));
	strlcpy(server_addr.sun_path, TEST_SUN_PATH,
	    sizeof(server_addr.sun_path));
	server_addr.sun_family = AF_UNIX;

	client_addr = server_addr;

	SOCKET(server_sd, PF_UNIX, SOCK_STREAM, 0);

	if (bind(server_sd, (struct sockaddr *) &server_addr,
	    sizeof(struct sockaddr_un)) == -1)
		test_fail("bind() should have worked");

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	fcntl(server_sd, F_SETFL, fcntl(server_sd, F_GETFL) | O_NONBLOCK);

	check_select(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	len = sizeof(addr);
	if (accept(server_sd, (struct sockaddr *) &addr, &len) != -1 ||
	    errno != EAGAIN)
		test_fail("accept() should have yielded EAGAIN");

	SOCKET(client_sd, PF_UNIX, SOCK_STREAM, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != -1 || errno != EINPROGRESS)
		test_fail("connect() should have yielded EINPROGRESS");

	check_select(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/);

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != -1 || errno != EALREADY)
		test_fail("connect() should have yielded EALREADY");

	if (recv(client_sd, buf, sizeof(buf), 0) != -1 || errno != EAGAIN)
		test_fail("recv() should have yielded EAGAIN");

	/* This may be an implementation aspect, or even plain wrong (?). */
	if (send(client_sd, buf, sizeof(buf), 0) != -1 || errno != EAGAIN)
		test_fail("send() should have yielded EAGAIN");

	switch (fork()) {
	case 0:
		close(client_sd);

		check_select(server_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

		len = sizeof(addr);
		client_sd = accept(server_sd, (struct sockaddr *) &addr, &len);
		if (client_sd == -1)
			test_fail("accept() should have succeeded");

		check_select(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

		close(server_sd);

		/* Let the socket become writable in the parent process. */
		sleep(1);

		if (write(client_sd, buf, 1) != 1)
			test_fail("write() should have succeeded");

		/* Wait for the client side to close. */
		check_select(client_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);
		check_select(client_sd, 1 /*read*/, -1 /*write*/, 1 /*block*/);
		check_select(client_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

		exit(errct);
	case -1:
		test_fail("can't fork");
	default:
		break;
	}

	close(server_sd);

	check_select(client_sd, 0 /*read*/, 1 /*write*/, 1 /*block*/);
	check_select(client_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != -1 || errno != EISCONN)
		test_fail("connect() should have yielded EISCONN");

	check_select(client_sd, 1 /*read*/, -1 /*write*/, 1 /*block*/);
	check_select(client_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

	if (read(client_sd, buf, 1) != 1)
		test_fail("read() should have succeeded");

	check_select(client_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	if (read(client_sd, buf, 1) != -1 || errno != EAGAIN)
		test_fail("read() should have yielded EAGAIN");

	/* Let the child process block on the select waiting for the close. */
	sleep(1);

	close(client_sd);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	UNLINK(TEST_SUN_PATH);
}

/*
 * Verify that a nonblocking connect for which there is an accepter, succeeds
 * immediately.  A pretty lame test, only here for completeness.
 */
static void
test_connect_nb(void)
{
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_un server_addr, client_addr, addr;
	int status;

	memset(&server_addr, 0, sizeof(server_addr));
	strlcpy(server_addr.sun_path, TEST_SUN_PATH,
	    sizeof(server_addr.sun_path));
	server_addr.sun_family = AF_UNIX;

	client_addr = server_addr;

	SOCKET(server_sd, PF_UNIX, SOCK_STREAM, 0);

	if (bind(server_sd, (struct sockaddr *) &server_addr,
	    sizeof(struct sockaddr_un)) == -1)
		test_fail("bind() should have worked");

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	switch (fork()) {
	case 0:
		len = sizeof(addr);
		if (accept(server_sd, (struct sockaddr *) &addr, &len) == -1)
			test_fail("accept() should have succeeded");

		exit(errct);
	case -1:
		test_fail("can't fork");
	default:
		break;
	}

	close(server_sd);

	sleep(1);

	SOCKET(client_sd, PF_UNIX, SOCK_STREAM, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != 0)
		test_fail("connect() should have succeeded");

	close(client_sd);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	UNLINK(TEST_SUN_PATH);
}

static void
dummy_handler(int sig)
{
	/* Nothing. */
}

/*
 * Verify that:
 * - interrupting a blocking connect will return EINTR but complete in the
 *   background later;
 * - doing a blocking write on an asynchronously connecting socket succeeds
 *   once the socket is connected.
 * - doing a nonblocking write on a connected socket with lots of pending data
 *   yields EAGAIN.
 */
static void
test_intr(void)
{
	struct sigaction act, oact;
	char buf[BUFSIZE];
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_un server_addr, client_addr, addr;
	int r, status;

	memset(buf, 0, sizeof(buf));

	memset(&server_addr, 0, sizeof(server_addr));
	strlcpy(server_addr.sun_path, TEST_SUN_PATH,
	    sizeof(server_addr.sun_path));
	server_addr.sun_family = AF_UNIX;

	client_addr = server_addr;

	SOCKET(server_sd, PF_UNIX, SOCK_STREAM, 0);

	if (bind(server_sd, (struct sockaddr *) &server_addr,
	    sizeof(struct sockaddr_un)) == -1)
		test_fail("bind() should have worked");

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	SOCKET(client_sd, PF_UNIX, SOCK_STREAM, 0);

	memset(&act, 0, sizeof(act));
	act.sa_handler = dummy_handler;
	if (sigaction(SIGALRM, &act, &oact) == -1)
		test_fail("sigaction() should have succeeded");

	alarm(1);

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != -1 || errno != EINTR)
		test_fail("connect() should have yielded EINTR");

	check_select(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/);

	switch (fork()) {
	case 0:
		close(client_sd);

		check_select(server_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

		len = sizeof(addr);
		client_sd = accept(server_sd, (struct sockaddr *) &addr, &len);
		if (client_sd == -1)
			test_fail("accept() should have succeeded");

		check_select(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

		close(server_sd);

		check_select(client_sd, 1 /*read*/, -1 /*write*/, 1 /*block*/);
		check_select(client_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

		if (recv(client_sd, buf, sizeof(buf), 0) != sizeof(buf))
			test_fail("recv() should have yielded bytes");

		/* No partial transfers should be happening. */
		check_select(client_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

		fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) |
		    O_NONBLOCK);

		/* We can only test nonblocking writes by filling the pipe. */
		while ((r = write(client_sd, buf, sizeof(buf))) > 0);

		if (r != -1 || errno != EAGAIN)
			test_fail("write() should have yielded EAGAIN");

		check_select(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/);

		if (write(client_sd, buf, 1) != -1 || errno != EAGAIN)
			test_fail("write() should have yielded EAGAIN");

		exit(errct);
	case -1:
		test_fail("can't fork");
	default:
		break;
	}

	close(server_sd);

	if (send(client_sd, buf, sizeof(buf), 0) != sizeof(buf))
		test_fail("send() should have succeded");

	check_select(client_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	check_select(client_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

	close(client_sd);

	sigaction(SIGALRM, &oact, NULL);

	UNLINK(TEST_SUN_PATH);
}

/*
 * Verify that closing a connecting socket before it is accepted will result in
 * no activity on the accepting side later.
 */
static void
test_connect_close(void)
{
	int server_sd, client_sd;
	struct sockaddr_un server_addr, client_addr;
	socklen_t len;

	memset(&server_addr, 0, sizeof(server_addr));
	strlcpy(server_addr.sun_path, TEST_SUN_PATH,
	    sizeof(server_addr.sun_path));
	server_addr.sun_family = AF_UNIX;

	client_addr = server_addr;

	SOCKET(server_sd, PF_UNIX, SOCK_STREAM, 0);

	if (bind(server_sd, (struct sockaddr *) &server_addr,
	    sizeof(struct sockaddr_un)) == -1)
		test_fail("bind() should have worked");

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	fcntl(server_sd, F_SETFL, fcntl(server_sd, F_GETFL) | O_NONBLOCK);

	check_select(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	SOCKET(client_sd, PF_UNIX, SOCK_STREAM, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != -1 || errno != EINPROGRESS)
		test_fail("connect() should have yielded EINPROGRESS");

	check_select(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/);
	check_select(server_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

	close(client_sd);

	check_select(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	len = sizeof(client_addr);
	if (accept(server_sd, (struct sockaddr *) &client_addr, &len) != -1 ||
	    errno != EAGAIN)
		test_fail("accept() should have yielded EAGAIN");

	close(server_sd);

	UNLINK(TEST_SUN_PATH);
}

/*
 * Verify that closing a listening socket will cause a blocking connect to fail
 * with ECONNRESET, and that a subsequent write will yield EPIPE.
 */
static void
test_listen_close(void)
{
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_un server_addr, client_addr, addr;
	int status;
	char byte;

	memset(&server_addr, 0, sizeof(server_addr));
	strlcpy(server_addr.sun_path, TEST_SUN_PATH,
	    sizeof(server_addr.sun_path));
	server_addr.sun_family = AF_UNIX;

	client_addr = server_addr;

	SOCKET(server_sd, PF_UNIX, SOCK_STREAM, 0);

	if (bind(server_sd, (struct sockaddr *) &server_addr,
	    sizeof(struct sockaddr_un)) == -1)
		test_fail("bind() should have worked");

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	switch (fork()) {
	case 0:
		sleep(1);

		exit(0);
	case -1:
		test_fail("can't fork");
	default:
		break;
	}

	close(server_sd);

	SOCKET(client_sd, PF_UNIX, SOCK_STREAM, 0);

	byte = 0;
	if (write(client_sd, &byte, 1) != -1 || errno != ENOTCONN)
		/* Yes, you fucked up the fix for the FIXME below. */
		test_fail("write() should have yielded ENOTCONN");

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != -1 || errno != ECONNRESET)
		test_fail("connect() should have yielded ECONNRESET");

	/*
	 * FIXME: currently UDS cannot distinguish between sockets that have
	 * not yet been connected, and sockets that have been disconnected.
	 * Thus, we get the same error for both: ENOTCONN instead of EPIPE.
	 */
#if 0
	if (write(client_sd, &byte, 1) != -1 || errno != EPIPE)
		test_fail("write() should have yielded EPIPE");
#endif

	close(client_sd);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	UNLINK(TEST_SUN_PATH);
}

/*
 * Verify that closing a listening socket will cause a nonblocking connect to
 * result in the socket becoming readable and writable, and yielding ECONNRESET
 * and EPIPE on the next two writes, respectively.
 */
static void
test_listen_close_nb(void)
{
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_un server_addr, client_addr, addr;
	int status;
	char byte;

	memset(&server_addr, 0, sizeof(server_addr));
	strlcpy(server_addr.sun_path, TEST_SUN_PATH,
	    sizeof(server_addr.sun_path));
	server_addr.sun_family = AF_UNIX;

	client_addr = server_addr;

	SOCKET(server_sd, PF_UNIX, SOCK_STREAM, 0);

	if (bind(server_sd, (struct sockaddr *) &server_addr,
	    sizeof(struct sockaddr_un)) == -1)
		test_fail("bind() should have worked");

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	switch (fork()) {
	case 0:
		sleep(1);

		exit(0);
	case -1:
		test_fail("can't fork");
	default:
		break;
	}

	close(server_sd);

	SOCKET(client_sd, PF_UNIX, SOCK_STREAM, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, (struct sockaddr *) &client_addr,
	    sizeof(struct sockaddr_un)) != -1 || errno != EINPROGRESS)
		test_fail("connect() should have yielded EINPROGRESS");

	check_select(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/);
	check_select(client_sd, 1 /*read*/, 1 /*write*/, 1 /*block*/);

	byte = 0;
	if (write(client_sd, &byte, 1) != -1 || errno != ECONNRESET)
		test_fail("write() should have yielded ECONNRESET");

	/*
	 * FIXME: currently UDS cannot distinguish between sockets that have
	 * not yet been connected, and sockets that have been disconnected.
	 * Thus, we get the same error for both: ENOTCONN instead of EPIPE.
	 */
#if 0
	if (write(client_sd, &byte, 1) != -1 || errno != EPIPE)
		test_fail("write() should have yielded EPIPE");
#endif

	check_select(client_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/);

	close(client_sd);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	UNLINK(TEST_SUN_PATH);
}

int main(int argc, char *argv[])
{
	int i;

	debug("entering main()");

	start(56);

	test_socket();
	test_bind();
	test_listen();
	test_getsockname();
	test_header();
	test_shutdown();
	test_close();
	test_permissions();
	test_dup();
	test_dup2();
	test_socketpair();
	test_shutdown();
	test_read();
	test_write();
	test_sockopts();
	test_ucred();
	test_xfer();

	for (i = 0; i < 3; i++) {
		test_simple_client_server(types[i]);
		if (types[i] != SOCK_DGRAM) test_vectorio(types[i]);
		if (types[i] != SOCK_DGRAM) test_msg(types[i]);
	}
	test_abort_client_server(1);
	test_abort_client_server(2);
	test_msg_dgram();
	test_connect();
	test_multiproc_read();
	test_multiproc_write();
	test_scm_credentials();
	test_fd_passing();
	test_select();
	test_select_close();
	test_fchmod();
	test_nonblock();
	test_connect_nb();
	test_intr();
	test_connect_close();
	test_listen_close();
	test_listen_close_nb();

	quit();

	return -1;	/* we should never get here */
}
