#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "common-socket.h"

#define ISO8601_FORMAT "%Y-%m-%dT%H:%M:%S"

/* timestamps for debug and error logs */
static char *get_timestamp(void)
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

void test_fail_fl(char *msg, char *file, int line)
{
	char *timestamp;
	int e;
	e = errno;
	timestamp = get_timestamp();
	if (errct == 0) fprintf(stderr, "\n");
	errno = e;
	fprintf(stderr, "[ERROR][%s] (%s Line %d) %s [pid=%d:errno=%d:%s]\n",
	    timestamp, file, line, msg, getpid(), errno, strerror(errno));
	fflush(stderr);
	if (timestamp != NULL) {
		free(timestamp);
		timestamp = NULL;
	}
	errno = e;
	e(7);
}

#if DEBUG == 1
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
#endif

void test_socket(const struct socket_test_info *info)
{
	struct stat statbuf, statbuf2;
	int sd, sd2;
	int rc;
	int i;

	debug("entering test_socket()");

	debug("Test socket() with an unsupported address family");

	errno = 0;
	sd = socket(-1, info->type, 0);
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
	sd = socket(info->domain, info->type, 0);
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
	sd = socket(info->domain, info->type, 4);
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

	SOCKET(sd, info->domain, info->type, 0);
	SOCKET(sd2, info->domain, info->type, 0);

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

void test_getsockname(const struct socket_test_info *info)
{
	int sd;
	int rc;
	int on;
	struct sockaddr_storage sock_addr;
	socklen_t sock_addr_len;

	SOCKET(sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
	if (rc == -1) {
		test_fail("bind() should have worked");
	}

	debug("Test getsockname() success");

	memset(&sock_addr, '\0', sizeof(sock_addr));
	sock_addr_len = sizeof(sock_addr);

	rc = getsockname(sd, (struct sockaddr *) &sock_addr, &sock_addr_len);
	if (rc == -1) {
		test_fail("getsockname() should have worked");
	}

	info->callback_check_sockaddr((struct sockaddr *) &sock_addr,
		sock_addr_len, "getsockname", 1);

	CLOSE(sd);
}

void test_bind(const struct socket_test_info *info)
{
	struct sockaddr_storage sock_addr;
	socklen_t sock_addr_len;
	int sd;
	int sd2;
	int rc;
	int on;

	debug("entering test_bind()");
	info->callback_cleanup();

	debug("Test bind() success");

	SOCKET(sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
	if (rc == -1) {
		test_fail("bind() should have worked");
	}

	debug("Test getsockname() success");

	memset(&sock_addr, '\0', sizeof(sock_addr));
	sock_addr_len = sizeof(sock_addr);

	rc = getsockname(sd, (struct sockaddr *) &sock_addr, &sock_addr_len);
	if (rc == -1) {
		test_fail("getsockname() should have worked");
	}

	info->callback_check_sockaddr((struct sockaddr *) &sock_addr,
		sock_addr_len, "getsockname", 1);

	debug("Test bind() with a address that has already been bind()'d");

	SOCKET(sd2, info->domain, info->type, 0);
	errno = 0;
	rc = bind(sd2, info->serveraddr, info->serveraddrlen);
	if (!((rc == -1) && (errno == EADDRINUSE))) {
		test_fail("bind() should have failed with EADDRINUSE");
	}
	CLOSE(sd2);
	CLOSE(sd);
	info->callback_cleanup();

	debug("Test bind() with a NULL address");

	SOCKET(sd, info->domain, info->type, 0);
	errno = 0;
	rc = bind(sd, (struct sockaddr *) NULL,
		sizeof(struct sockaddr_storage));
	if (!((rc == -1) && (errno == EFAULT))) {
		test_fail("bind() should have failed with EFAULT");
	}
	CLOSE(sd);

	debug("leaving test_bind()");
}

void test_listen(const struct socket_test_info *info)
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

void test_shutdown(const struct socket_test_info *info)
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
		if (!(rc == -1 && errno == ENOTSOCK)) {
			test_fail("shutdown() should have failed with "
			    "ENOTSOCK");
		}

		debug("test shutdown() with a socket that is not connected");

		SOCKET(sd, info->domain, info->type, 0);
		errno = 0;
		rc = shutdown(sd, how[i]);
		if (rc != 0 && !(rc == -1 && errno == ENOTCONN)) {
			test_fail("shutdown() should have failed");
		}
		CLOSE(sd);
	}

	SOCKET(sd, info->domain, info->type, 0);
	errno = 0;
	rc = shutdown(sd, -1);
	if (!(rc == -1 && errno == EINVAL)) {
		test_fail("shutdown(sd, -1) should have failed with EINVAL");
	}
	CLOSE(sd);

	debug("leaving test_shutdown()");
}

void test_close(const struct socket_test_info *info)
{
	int sd, sd2;
	int rc, i, on;

	debug("entering test_close()");

	info->callback_cleanup();

	debug("Test close() success");

	SOCKET(sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
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

	info->callback_cleanup();

	debug("dup()'ing a file descriptor and closing both should work");

	SOCKET(sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
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

	info->callback_cleanup();

	/* Create and close a socket a bunch of times.
	 * If the implementation doesn't properly free the
	 * socket during close(), eventually socket() will
	 * fail when the internal descriptor table is full.
	 */
	for (i = 0; i < 1024; i++) {
		SOCKET(sd, info->domain, info->type, 0);
		CLOSE(sd);
	}

	debug("leaving test_close()");
}

void test_sockopts(const struct socket_test_info *info)
{
	int i;
	int rc;
	int sd;
	int option_value;
	socklen_t option_len;

	debug("entering test_sockopts()");

	for (i = 0; i < info->typecount; i++) {

		SOCKET(sd, info->domain, info->types[i], 0);

		debug("Test setsockopt() works");

		option_value = 0;
		option_len = sizeof(option_value);
		errno = 0;
		rc = getsockopt(sd, SOL_SOCKET, SO_TYPE, &option_value,
							&option_len);
		if (rc != 0) {
			test_fail("setsockopt() should have worked");
		}

		if (option_value != info->types[i]) {
			test_fail("SO_TYPE didn't seem to work.");
		}

		CLOSE(sd);
	}

	SOCKET(sd, info->domain, info->type, 0);

	debug("Test setsockopt() works");

	option_value = 0;
	option_len = sizeof(option_value);
	errno = 0;
	rc = getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &option_value, &option_len);

	if (info->expected_sndbuf >= 0 &&
		option_value != info->expected_sndbuf) {
		test_fail("SO_SNDBUF didn't seem to work.");
	}

	CLOSE(sd);


	SOCKET(sd, info->domain, info->type, 0);

	debug("Test setsockopt() works");

	option_value = 0;
	option_len = sizeof(option_value);
	errno = 0;
	rc = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &option_value, &option_len);
	if (rc != 0) {
		test_fail("getsockopt() should have worked");
	}

	if (info->expected_rcvbuf >= 0 &&
		option_value != info->expected_rcvbuf) {
		test_fail("SO_RCVBUF didn't seem to work.");
	}

	CLOSE(sd);


	debug("leaving test_sockopts()");
}

void test_read(const struct socket_test_info *info)
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

void test_write(const struct socket_test_info *info)
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

void test_dup(const struct socket_test_info *info)
{
	struct stat info1;
	struct stat info2;
	int sd, sd2;
	int rc;
	int i, on;

	debug("entering test_dup()");

	info->callback_cleanup();

	debug("Test dup()");

	SOCKET(sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
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

	SOCKET(sd, info->domain, info->type, 0);

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

	info->callback_cleanup();

	debug("leaving test_dup()");
}

void test_dup2(const struct socket_test_info *info)
{
	struct stat info1;
	struct stat info2;
	int sd;
	int fd;
	int rc;
	int on;

	debug("entering test_dup2()");
	info->callback_cleanup();

	SOCKET(sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
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

	info->callback_cleanup();
	debug("leaving test_dup2()");

}

/*
 * A toupper() server. This toy server converts a string to upper case.
 */
static void test_xfer_server(const struct socket_test_info *info, pid_t pid)
{
	int i;
	struct timeval tv;
	fd_set readfds;
	int status;
	int rc;
	int sd;
	int on;
	unsigned char buf[BUFSIZE];
	socklen_t client_addr_size;
	int client_sd;
	struct sockaddr_storage client_addr;

	status = 0;
	rc = 0;
	sd = 0;
	client_sd = 0;
	client_addr_size = sizeof(struct sockaddr_storage);

	memset(&buf, '\0', sizeof(buf));
	memset(&client_addr, '\0', sizeof(client_addr));

	SOCKET(sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
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

	if (rc < strlen((char *)buf)) {
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

	if (rc < strlen((char *)buf)) {
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

	if (rc < strlen((char *)buf)) {
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
static void test_xfer_client(const struct socket_test_info *info)
{
	struct timeval tv;
	fd_set readfds;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	int sd;
	int rc;
	char buf[BUFSIZE];

	debug("[client] entering test_xfer_client()");
	errct = 0;	/* reset error count */
	memset(&buf, '\0', sizeof(buf));

	while (server_ready == 0) {
		debug("[client] waiting for the server to signal");
		sleep(1);
	}

	peer_addr_len = sizeof(peer_addr);


	if (info->callback_xfer_prepclient) info->callback_xfer_prepclient();

	debug("[client] creating client socket");
	SOCKET(sd, info->domain, info->type, 0);

	debug("[client] connecting to server through the symlink");
	rc = connect(sd, info->clientaddrsym, info->clientaddrsymlen);
	if (rc == -1) {
		test_fail("[client] connect() should have worked");
	} else {
		debug("[client] connected");
	}

	debug("[client] testing getpeername()");
	memset(&peer_addr, '\0', sizeof(peer_addr));
	rc = getpeername(sd, (struct sockaddr *) &peer_addr, &peer_addr_len);
	if (rc == -1) {
		test_fail("[client] getpeername() should have worked");
	}


	info->callback_check_sockaddr((struct sockaddr *) &peer_addr,
		peer_addr_len, "getpeername", 1);

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

	if (info->callback_xfer_peercred) info->callback_xfer_peercred(sd);

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

void test_xfer(const struct socket_test_info *info)
{
	pid_t pid;

	debug("entering test_xfer()");
	info->callback_cleanup();

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
		errct = 0;
		test_xfer_client(info);
		test_fail("we should never get here");
		exit(1);
	} else {
		debug("parent");
		test_xfer_server(info, pid);
		debug("parent done");
	}

	info->callback_cleanup();
	debug("leaving test_xfer()");
}

static void test_simple_client(const struct socket_test_info *info, int type)
{
	char buf[BUFSIZE];
	int sd, rc;

	sd = socket(info->domain, type, 0);
	if (sd == -1) {
		test_fail("socket");
		exit(errct);
	}

	while (server_ready == 0) {
		debug("[client] waiting for the server");
		sleep(1);
	}

	bzero(buf, BUFSIZE);
	snprintf(buf, BUFSIZE-1, "Hello, My Name is Client.");

	if (type == SOCK_DGRAM) {

		rc = sendto(sd, buf, strlen(buf) + 1, 0,
			info->clientaddr, info->clientaddrlen);
		if (rc == -1) {
			test_fail("sendto");
			exit(errct);
		}

	} else {

		rc = connect(sd, info->clientaddr, info->clientaddrlen);
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

static void test_simple_server(const struct socket_test_info *info, int type,
	pid_t pid)
{
	char buf[BUFSIZE];
	int sd, rc, client_sd, status, on;
	struct sockaddr_storage addr;
	socklen_t addr_len;

	addr_len = info->clientaddrlen;

	sd = socket(info->domain, type, 0);
	if (sd == -1) {
		test_fail("socket");
	}

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	assert(info->clientaddrlen <= sizeof(addr));
	memcpy(&addr, info->clientaddr, info->clientaddrlen);

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
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

static void test_abort_client(const struct socket_test_info *info,
	int abort_type);
static void test_abort_server(const struct socket_test_info *info,
	pid_t pid, int abort_type);

void test_abort_client_server(const struct socket_test_info *info,
	int abort_type)
{
	pid_t pid;

	debug("test_simple_client_server()");

	info->callback_cleanup();

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
		errct = 0;
		test_abort_client(info, abort_type);
		test_fail("we should never get here");
		exit(1);
	} else {
		debug("parent");
		test_abort_server(info, pid, abort_type);
		debug("parent done");
	}

	info->callback_cleanup();
}

static void test_abort_client(const struct socket_test_info *info,
	int abort_type)
{
	char buf[BUFSIZE];
	int sd, rc;

	sd = socket(info->domain, info->type, 0);
	if (sd == -1) {
		test_fail("socket");
		exit(errct);
	}

	while (server_ready == 0) {
		debug("[client] waiting for the server");
		sleep(1);
	}

	bzero(buf, BUFSIZE);
	snprintf(buf, BUFSIZE-1, "Hello, My Name is Client.");

	rc = connect(sd, info->clientaddr, info->clientaddrlen);
	if (rc == -1) {
		test_fail("connect");
		exit(errct);
	}

	if (abort_type == 2) {
		/* Give server a chance to close connection */
		sleep(2);
		rc = write(sd, buf, strlen(buf) + 1);
		if (rc != -1) {
			if (!info->ignore_write_conn_reset) {
				test_fail("write should have failed\n");
			}
		} else if (errno != EPIPE && errno != ECONNRESET) {
			test_fail("errno should've been EPIPE/ECONNRESET\n");
		}
	}

	rc = close(sd);
	if (rc == -1) {
		test_fail("close");
	}

	exit(errct);
}

static void test_abort_server(const struct socket_test_info *info,
	pid_t pid, int abort_type)
{
	char buf[BUFSIZE];
	int sd, rc, client_sd, status, on;
	struct sockaddr_storage addr;
	socklen_t addr_len;

	addr_len = info->clientaddrlen;

	sd = socket(info->domain, info->type, 0);
	if (sd == -1) {
		test_fail("socket");
	}

	on = 1;
	(void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	assert(sizeof(addr) >= info->clientaddrlen);
	memcpy(&addr, info->clientaddr, info->clientaddrlen);

	rc = bind(sd, info->serveraddr, info->serveraddrlen);
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
		if (rc != 0 && rc != -1) {
			test_fail("read should've failed or returned zero\n");
		}
		if (rc != 0 && errno != ECONNRESET) {
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

void test_simple_client_server(const struct socket_test_info *info, int type)
{
	pid_t pid;

	debug("entering test_simple_client_server()");

	info->callback_cleanup();

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
		errct = 0;
		test_simple_client(info, type);
		test_fail("we should never get here");
		exit(1);
	} else {
		debug("parent");
		test_simple_server(info, type, pid);
		debug("parent done");
	}

	info->callback_cleanup();
	debug("leaving test_simple_client_server()");
}

void test_msg_dgram(const struct socket_test_info *info)
{
	int rc;
	int src;
	int dst;
	struct sockaddr_storage addr;
	struct iovec iov[3];
	struct msghdr msg1;
	struct msghdr msg2;
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	char buf3[BUFSIZE];

	debug("entering test_msg_dgram");

	info->callback_cleanup();

	src = socket(info->domain, SOCK_DGRAM, 0);
	if (src == -1) {
		test_fail("socket");
	}

	dst = socket(info->domain, SOCK_DGRAM, 0);
	if (dst == -1) {
		test_fail("socket");
	}

	rc = bind(src, info->serveraddr2, info->serveraddr2len);
	if (rc == -1) {
		test_fail("bind");
	}

	assert(info->clientaddrlen <= sizeof(addr));
	memcpy(&addr, info->clientaddr, info->clientaddrlen);

	rc = bind(dst, info->serveraddr, info->serveraddrlen);
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
	msg1.msg_namelen = info->clientaddrlen;
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

	memset(&addr, '\0', sizeof(addr));
	memset(&msg2, '\0', sizeof(struct msghdr));
	msg2.msg_name = &addr;
	msg2.msg_namelen = sizeof(addr);
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

	info->callback_check_sockaddr((struct sockaddr *) &addr,
		msg2.msg_namelen, "recvmsg", 2);

	rc = close(dst);
	if (rc == -1) {
		test_fail("close");
	}

	rc = close(src);
	if (rc == -1) {
		test_fail("close");
	}

	info->callback_cleanup();
	debug("leaving test_msg_dgram");
}

#define check_select(sd, rd, wr, block) \
	check_select_internal(sd, rd, wr, block, 1, __LINE__)
#define check_select_cond(sd, rd, wr, block, allchecks) \
	check_select_internal(sd, rd, wr, block, allchecks, __LINE__)

static void
check_select_internal(int sd, int rd, int wr, int block, int allchecks, int line)
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

	errno = 0;
	if (select(sd + 1, &read_set, &write_set, NULL, &tv) < 0)
		test_fail_fl("select() failed unexpectedly", __FILE__, line);

	if (rd != -1 && !!FD_ISSET(sd, &read_set) != rd && allchecks)
		test_fail_fl("select() mismatch on read operation",
			__FILE__, line);

	if (wr != -1 && !!FD_ISSET(sd, &write_set) != wr && allchecks)
		test_fail_fl("select() mismatch on write operation",
			__FILE__, line);
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
void
test_nonblock(const struct socket_test_info *info)
{
	char buf[BUFSIZE];
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_storage addr;
	int status, on;

	debug("entering test_nonblock()");
	memset(buf, 0, sizeof(buf));

	SOCKET(server_sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(server_sd, info->serveraddr, info->serveraddrlen) == -1)
		test_fail("bind() should have worked");

	if (info->callback_set_listen_opt != NULL)
		info->callback_set_listen_opt(server_sd);

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	fcntl(server_sd, F_SETFL, fcntl(server_sd, F_GETFL) | O_NONBLOCK);

	check_select(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	len = sizeof(addr);
	if (accept(server_sd, (struct sockaddr *) &addr, &len) != -1 ||
	    errno != EAGAIN)
		test_fail("accept() should have yielded EAGAIN");

	SOCKET(client_sd, info->domain, info->type, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != -1) {
		test_fail("connect() should have failed");
	} else if (errno != EINPROGRESS) {
		test_fail("connect() should have yielded EINPROGRESS");
	}

	check_select_cond(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/,
		!info->ignore_select_delay);

	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != -1) {
		test_fail("connect() should have failed");
	} else if (errno != EALREADY && errno != EISCONN) {
		test_fail("connect() should have yielded EALREADY");
	}

	if (recv(client_sd, buf, sizeof(buf), 0) != -1 || errno != EAGAIN)
		test_fail("recv() should have yielded EAGAIN");

	/* This may be an implementation aspect, or even plain wrong (?). */
	if (!info->ignore_send_waiting) {
		if (send(client_sd, buf, sizeof(buf), 0) != -1) {
			test_fail("send() should have failed");
		} else if (errno != EAGAIN) {
			test_fail("send() should have yielded EAGAIN");
		}
	}

	switch (fork()) {
	case 0:
		errct = 0;
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
		check_select_cond(client_sd, 0 /*read*/, 1 /*write*/,
			0 /*block*/, !info->ignore_select_delay /*allchecks*/);
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

	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != -1 ||
		errno != EISCONN)
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

	errno = 0;
	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	info->callback_cleanup();
	debug("leaving test_nonblock()");
}

/*
 * Verify that a nonblocking connect for which there is an accepter, succeeds
 * immediately.  A pretty lame test, only here for completeness.
 */
void
test_connect_nb(const struct socket_test_info *info)
{
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_storage addr;
	int status, on;

	debug("entering test_connect_nb()");
	SOCKET(server_sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(server_sd, info->serveraddr, info->serveraddrlen) == -1)
		test_fail("bind() should have worked");

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	switch (fork()) {
	case 0:
		errct = 0;
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

	SOCKET(client_sd, info->domain, info->type, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != 0) {
		if (!info->ignore_connect_delay) {
			test_fail("connect() should have succeeded");
		} else if (errno != EINPROGRESS) {
			test_fail("connect() should have succeeded or "
				"failed with EINPROGRESS");
		}
	}

	close(client_sd);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	info->callback_cleanup();
	debug("leaving test_connect_nb()");
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
void
test_intr(const struct socket_test_info *info)
{
	struct sigaction act, oact;
	char buf[BUFSIZE];
	int isconn;
	socklen_t len;
	int server_sd, client_sd;
	struct sockaddr_storage addr;
	int r, status, on;

	debug("entering test_intr()");
	memset(buf, 0, sizeof(buf));

	SOCKET(server_sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(server_sd, info->serveraddr, info->serveraddrlen) == -1)
		test_fail("bind() should have worked");

	if (info->callback_set_listen_opt != NULL)
		info->callback_set_listen_opt(server_sd);

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	SOCKET(client_sd, info->domain, info->type, 0);

	memset(&act, 0, sizeof(act));
	act.sa_handler = dummy_handler;
	if (sigaction(SIGALRM, &act, &oact) == -1)
		test_fail("sigaction() should have succeeded");

	if (info->domain != PF_INET) alarm(1);

	isconn = 0;
	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != -1) {
		if (!info->ignore_connect_unaccepted) {
			test_fail("connect() should have failed");
		}
		isconn = 1;
	} else if (errno != EINTR) {
		test_fail("connect() should have yielded EINTR");
	}

	alarm(0);

	check_select(client_sd, 0 /*read*/, isconn /*write*/, 0 /*block*/);

	switch (fork()) {
	case 0:
		errct = 0;
		close(client_sd);

		/* Ensure that the parent is blocked on the send(). */
		sleep(1);

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

		sleep(1);

		fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) |
		    O_NONBLOCK);

		/* We can only test nonblocking writes by filling the pipe. */
		while ((r = write(client_sd, buf, sizeof(buf))) > 0);

		if (r != -1) {
			test_fail("write() should have failed");
		} else if (errno != EAGAIN) {
			test_fail("write() should have yielded EAGAIN");
		}

		check_select(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/);

		if (write(client_sd, buf, 1) != -1) {
			test_fail("write() should have failed");
		} else if (errno != EAGAIN) {
			test_fail("write() should have yielded EAGAIN");
		}

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

	info->callback_cleanup();
	debug("leaving test_intr()");
}

/*
 * Verify that closing a connecting socket before it is accepted will result in
 * no activity on the accepting side later.
 */
void
test_connect_close(const struct socket_test_info *info)
{
	int server_sd, client_sd, sd, on;
	struct sockaddr_storage addr;
	socklen_t len;

	debug("entering test_connect_close()");
	SOCKET(server_sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(server_sd, info->serveraddr, info->serveraddrlen) == -1)
		test_fail("bind() should have worked");

	if (info->callback_set_listen_opt != NULL)
		info->callback_set_listen_opt(server_sd);

	if (listen(server_sd, 8) == -1)
		test_fail("listen() should have worked");

	fcntl(server_sd, F_SETFL, fcntl(server_sd, F_GETFL) | O_NONBLOCK);

	check_select(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/);

	SOCKET(client_sd, info->domain, info->type, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != -1 ||
		errno != EINPROGRESS)
		test_fail("connect() should have yielded EINPROGRESS");

	check_select_cond(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/,
		!info->ignore_select_delay);
	check_select_cond(server_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/,
		!info->ignore_select_delay);

	close(client_sd);

	check_select_cond(server_sd, 0 /*read*/, 1 /*write*/, 0 /*block*/,
		!info->ignore_select_delay);

	len = sizeof(addr);
	errno = 0;
	if ((sd = accept(server_sd, (struct sockaddr *) &addr, &len)) != -1) {
		if (!info->ignore_accept_delay) {
			test_fail("accept() should have failed");
		}
		close(sd);
	} else if (errno != EAGAIN) {
		test_fail("accept() should have yielded EAGAIN");
	}
	close(server_sd);

	info->callback_cleanup();
	debug("leaving test_connect_close()");
}

/*
 * Verify that closing a listening socket will cause a blocking connect to fail
 * with ECONNRESET, and that a subsequent write will yield EPIPE.  This test
 * works only if the connect(2) does not succeed before accept(2) is called at
 * all, which means it is limited to UDS with LOCAL_CONNWAIT right now.
 */
void
test_listen_close(const struct socket_test_info *info)
{
	int server_sd, client_sd;
	int status, on;
	char byte;

	debug("entering test_listen_close()");
	SOCKET(server_sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(server_sd, info->serveraddr, info->serveraddrlen) == -1)
		test_fail("bind() should have worked");

	if (info->callback_set_listen_opt != NULL)
		info->callback_set_listen_opt(server_sd);

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

	SOCKET(client_sd, info->domain, info->type, 0);

	byte = 0;
	if (write(client_sd, &byte, 1) != -1 || errno != ENOTCONN)
		test_fail("write() should have yielded ENOTCONN");

	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != -1) {
		test_fail("connect() should have failed");
	} else if (errno != ECONNRESET) {
		test_fail("connect() should have yielded ECONNRESET");
	}

	/*
	 * The error we get on the next write() depends on whether the socket
	 * may be reused after a failed connect.  For UDS, it may be, so we get
	 * ENOTCONN.  Otherwise we would expect EPIPE.
	 */
	if (write(client_sd, &byte, 1) != -1 || errno != ENOTCONN)
		test_fail("write() should have yielded ENOTCONN");

	close(client_sd);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	info->callback_cleanup();
	debug("leaving test_listen_close()");
}

/*
 * Verify that closing a listening socket will cause a nonblocking connect to
 * result in the socket becoming readable and writable, and yielding ECONNRESET
 * and EPIPE on the next two writes, respectively.
 */
void
test_listen_close_nb(const struct socket_test_info *info)
{
	int server_sd, client_sd;
	int status, on;
	char byte;

	debug("entering test_listen_close_nb()");
	SOCKET(server_sd, info->domain, info->type, 0);

	on = 1;
	(void)setsockopt(server_sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(server_sd, info->serveraddr, info->serveraddrlen) == -1)
		test_fail("bind() should have worked");

	if (info->callback_set_listen_opt != NULL)
		info->callback_set_listen_opt(server_sd);

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

	SOCKET(client_sd, info->domain, info->type, 0);

	fcntl(client_sd, F_SETFL, fcntl(client_sd, F_GETFL) | O_NONBLOCK);

	if (connect(client_sd, info->clientaddr, info->clientaddrlen) != -1 ||
		errno != EINPROGRESS)
		test_fail("connect() should have yielded EINPROGRESS");

	check_select_cond(client_sd, 0 /*read*/, 0 /*write*/, 0 /*block*/,
		!info->ignore_select_delay);
	check_select_cond(client_sd, 1 /*read*/, 1 /*write*/, 1 /*block*/,
		!info->ignore_select_delay);

	byte = 0;
	if (write(client_sd, &byte, 1) != -1) {
		if (!info->ignore_write_conn_reset) {
			test_fail("write() should have failed");
		}
	} else if (errno != ECONNRESET) {
		test_fail("write() should have yielded ECONNRESET");
	}

	check_select_cond(client_sd, 1 /*read*/, 1 /*write*/, 0 /*block*/,
		!info->ignore_select_delay);

	close(client_sd);

	if (wait(&status) <= 0)
		test_fail("wait() should have succeeded");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		test_fail("child process failed the test");

	info->callback_cleanup();
	debug("leaving test_listen_close_nb()");
}
