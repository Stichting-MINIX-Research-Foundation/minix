#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"

#define CLOEXEC_PORT 3490
#define FORK_PORT 3491

static int fd = 0;

void copy_subtests(void);
void test_open_file_cloexec(void);
void test_open_file_fork(void);
void test_open_socket_cloexec(void);
void test_open_socket_fork(void);
void start_socket_server(int port);
int start_socket_client(int port, int flag);

void
copy_subtests()
{
	char *subtests[] = { "t67a", "t67b" };
	char copy_cmd[8 + PATH_MAX + 1];
	int i, no_tests;

	no_tests = sizeof(subtests) / sizeof(char *);

	for (i = 0; i < no_tests; i++) {
		snprintf(copy_cmd, 8 + PATH_MAX, "cp ../%s .", subtests[i]);
		system(copy_cmd);
	}
}

void
test_open_file_cloexec()
{
	int flags;
	pid_t pid;

	/* Let's create a file with O_CLOEXEC turned on */
	fd = open("file", O_RDWR|O_CREAT|O_CLOEXEC, 0660);
	if (fd < 0) e(1);

	/* Now verify through fcntl the flag is indeed set */
	flags = fcntl(fd, F_GETFD);
	if (flags < 0) e(2);
	if (!(flags & FD_CLOEXEC)) e(3);

	/* Fork a child and let child exec a test program that verifies
	 * fd is not a valid file */
	pid = fork();
	if (pid == -1) e(4);	
	else if (pid == 0) {
		/* We're the child */
		char fd_buf[2];

		/* Verify again O_CLOEXEC is on */
		flags = fcntl(fd, F_GETFD);
		if (flags < 0) e(5);
		if (!(flags & FD_CLOEXEC)) e(6);

		snprintf(fd_buf, sizeof(fd_buf), "%d", fd);
		execl("./t67b", "t67b", fd_buf, NULL);

		/* Should not reach this */
		exit(1);
	} else {
		/* We're the parent */
		int result;

		if (waitpid(pid, &result, 0) == -1) e(7);
		if (WEXITSTATUS(result) != 0) e(8);
	}
	close(fd);
}

void
test_open_file_fork()
{
	int flags;
	pid_t pid;

	/* Let's create a file with O_CLOEXEC NOT turned on */
	fd = open("file", O_RDWR|O_CREAT, 0660);
	if (fd < 0) e(1);

	/* Now verify through fcntl the flag is indeed not set */
	flags = fcntl(fd, F_GETFD);
	if (flags < 0) e(2);
	if (flags & FD_CLOEXEC) e(3);

	/* Fork a child and let child exec a test program that verifies
	 * fd is a valid file */
	pid = fork();
	if (pid == -1) e(4);	
	else if (pid == 0) {
		/* We're the child */
		char fd_buf[2];

		/* Verify again O_CLOEXEC is off */
		flags = fcntl(fd, F_GETFD);
		if (flags < 0) e(5);
		if (flags & FD_CLOEXEC) e(6);

		snprintf(fd_buf, sizeof(fd_buf), "%d", fd);
		execl("./t67a", "t67a", fd_buf, NULL);

		/* Should not reach this */
		exit(1);
	} else {
		/* We're the parent */
		int result = 0;

		if (waitpid(pid, &result, 0) == -1) e(7);
		if (WEXITSTATUS(result) != 0) e(8);
	}
	close(fd);
}

int
start_socket_client(int port, int flag)
{
	int fd_sock;
	struct hostent *he;
	struct sockaddr_in server;

	if ((fd_sock = socket(PF_INET, SOCK_STREAM|flag, 0)) < 0) {
		perror("Error obtaining socket\n");
		e(1);
	}

	if ((he = gethostbyname("127.0.0.1")) == NULL) {
		perror("Error retrieving home\n");
		e(2);
	}

	/* Copy server host result */
	memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	/* Normally, we'd zerofill sin_zero, but there is no such thing on
	 * Minix at the moment */
#if !defined(__minix)
	memset(&server.sin_zero, '\0', sizeof(server.sin_zero));
#endif
	
	if (connect(fd_sock, (struct sockaddr *) &server, sizeof(server)) < 0){
		perror("Error connecting to server\n");
		e(3);
	}

	return fd_sock;
}


void
start_socket_server(int port)
{
#if !defined(__minix)
	int yes = 1;
#endif
	int fd_sock, fd_new, r;
	struct sockaddr_in my_addr;
	struct sockaddr_in other_addr;
	socklen_t other_size;
	char buf[1];

	if ((fd_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Error getting socket\n");
		e(1);
	}

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	/* Normally we'd zerofill sin_zero, but current Minix socket interface
	 * does not support that field */
#if !defined(__minix)
	memset(&my_addr.sin_zero, '\0', sizeof(sin.sin_zero));
#endif
	
	/* Reuse port number when invoking test often */
#if !defined(__minix)
	if (setsockopt(fd_sock, SOL_SOCKET, SO_REUSEADDR, &yes,
	    sizeof(int)) < 0) {
		perror("Error setting port reuse option\n");
		e(2);
	}
#endif

	/* Bind to port */
	if (bind(fd_sock, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
		perror("Error binding to port\n");
		e(3);
	}

	/* Set socket in listening mode */
	if (listen(fd_sock, 20) < 0) {
		perror("Error listening for incoming connections");
		e(4);
	}

	/* Accept incoming connections */
	fd_new = accept(fd_sock, (struct sockaddr *) &other_addr, &other_size);

	if (fd_new < 0) {
		perror("Error accepting new connections\n");
		e(5);
	}

	r = read(fd_new, buf, sizeof(buf));
	exit(0);
}

void
test_open_socket_cloexec()
{
/* This subtest will start a server and client using TCP. The client will
 * open the socket with SOCK_CLOEXEC turned on, so that after a fork+exec, the
 * socket should become invalid.
 *                o
 *              / | 
 *        server  |
 *      (accept)  |
 *             |  | \
 *             |  |  client
 *             |  |  (connect)
 *             |  |  | \
 *             |  |  |  client_fork
 *             |  |  |  (exec t67b)
 *        (read)  |  |  (write)
 *             |  |  | /
 *             |  |  (waitpid client_fork) 
 *              \ |  |
 * (waitpid server)  |
 *                | /
 * (waitpid client)
 *                |
 *                o
 */
	pid_t pid_server, pid_client;
	int result;

	pid_server = fork();
	if (pid_server < 0) e(1);
	if (pid_server == 0) {
		start_socket_server(CLOEXEC_PORT);
		return; /* Never reached */
	}

	pid_client = fork();
	if (pid_client < 0) e(2);
	if (pid_client == 0) {
		pid_t pid_client_fork;
		int sockfd;

		sockfd = start_socket_client(CLOEXEC_PORT, SOCK_CLOEXEC);
		if (sockfd < 0) e(4);

		pid_client_fork = fork();
		if (pid_client_fork < 0) {
			e(5);
			exit(5);
		}
		if (pid_client_fork == 0) {
			/* We're a fork of the client. After we exec, the
			 * socket should become invalid due to the SOCK_CLOEXEC
			 * flag.
			 */
			char sockfd_buf[2];
			int flags;

			/* Verify O_CLOEXEC is on */
			flags = fcntl(sockfd, F_GETFD);
			if (flags < 0) e(5);
			if (!(flags & FD_CLOEXEC)) e(6);

			/* t67b will verify that it can't write to sockfd and
			 * that opening a new file will yield a file descriptor
			 * with the same number.
			 */
			snprintf(sockfd_buf, sizeof(sockfd_buf), "%d", sockfd);
			execl("./t67b", "t67b", sockfd_buf, NULL);

			/* Should not reach this */
			exit(1);
		} else {
			if (waitpid(pid_client_fork, &result, 0) < 0) e(8);
			exit(WEXITSTATUS(result)); /* Pass on error to main */
		}
		exit(0);	/* Never reached */
	}

	if (waitpid(pid_server, &result, 0) < 0) e(3);
	if (waitpid(pid_client, &result, 0) < 0) e(4);

	/* Let's inspect client result */
	if (WEXITSTATUS(result) != 0) e(5);
}

void
test_open_socket_fork(void)
{
/* This subtest will start a server and client using TCP. The client will
 * open the socket with SOCK_CLOEXEC turned off, so that after a fork+exec, the
 * socket should stay valid.
 *                o
 *              / | 
 *        server  |
 *      (accept)  |
 *             |  | \
 *             |  |  client
 *             |  |  (connect)
 *             |  |  | \
 *             |  |  |  client_fork
 *             |  |  |  (exec t67a)
 *        (read)  |  |  (write)
 *             |  |  | /
 *             |  |  (waitpid client_fork) 
 *              \ |  |
 * (waitpid server)  |
 *                | /
 * (waitpid client)
 *                |
 *                o
 */
	pid_t pid_server, pid_client;
	int result;

	pid_server = fork();
	if (pid_server < 0) e(1);
	if (pid_server == 0) {
		start_socket_server(FORK_PORT);
		return; /* Never reached */
	}

	pid_client = fork();
	if (pid_client < 0) e(2);
	if (pid_client == 0) {
		pid_t pid_client_fork;
		int sockfd;

		sockfd = start_socket_client(FORK_PORT, 0);
		if (sockfd < 0) e(4);

		pid_client_fork = fork();
		if (pid_client_fork < 0) {
			e(5);
			exit(5);
		}
		if (pid_client_fork == 0) {
			/* We're a fork of the client. After we exec, the
			 * socket should stay valid due to lack of SOCK_CLOEXEC
			 * flag.
			 */
			char sockfd_buf[2];
			int flags;

			/* Verify O_CLOEXEC is off */
			flags = fcntl(sockfd, F_GETFD);
			if (flags < 0) e(5);
			if (flags & FD_CLOEXEC) e(6);

			/* t67a will verify that it can't write to sockfd and
			 * that opening a new file will yield a file descriptor
			 * with a higher number.
			 */
			snprintf(sockfd_buf, sizeof(sockfd_buf), "%d", sockfd);
			execl("./t67a", "t67a", sockfd_buf, NULL);

			/* Should not reach this */
			exit(1);
		} else {
			if (waitpid(pid_client_fork, &result, 0) < 0) e(8);
			exit(WEXITSTATUS(result)); /* Pass on error to main */
		}
		exit(0);	/* Never reached */
	}

	if (waitpid(pid_server, &result, 0) < 0) e(3);
	if (waitpid(pid_client, &result, 0) < 0) e(4);

	/* Let's inspect client result */
	if (WEXITSTATUS(result) != 0) e(5);
}

int
main(int argc, char *argv[])
{
	start(67);
	copy_subtests();
	test_open_file_fork();
	test_open_file_cloexec();
	test_open_socket_fork();
	test_open_socket_cloexec();
	quit();
	return(-1);	/* Unreachable */
}

