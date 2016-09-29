#define DEBUG 0

/* buffer for send/recv */
#define BUFSIZE (128)

/* macro to display information about a failed test and increment the errct */
void test_fail_fl(char *msg, char *file, int line);
#define test_fail(msg)	test_fail_fl(msg, __FILE__, __LINE__)

#if DEBUG == 1
/* macros to display debugging information */
void debug_fl(char *msg, char *file, int line);
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

extern int server_ready;
void test_xfer_sighdlr(int sig);

struct socket_test_info {
	const struct sockaddr *clientaddr;
	socklen_t clientaddrlen;
	const struct sockaddr *clientaddr2;
	socklen_t clientaddr2len;
	const struct sockaddr *clientaddrsym;
	socklen_t clientaddrsymlen;
	int domain;
	int expected_rcvbuf;
	int expected_sndbuf;
	const struct sockaddr *serveraddr;
	socklen_t serveraddrlen;
	const struct sockaddr *serveraddr2;
	socklen_t serveraddr2len;
	int type;
	const int *types;
	size_t typecount;

	int ignore_accept_delay; /* success from accept after aborted connect */
	int ignore_connect_delay; /* nb connect not instant */
	int ignore_connect_unaccepted; /* connect succeeds without accept */
	int ignore_select_delay; /* select delay reflecting other side nb op */
	int ignore_send_waiting; /* can send while waiting for nb recv */
	int ignore_write_conn_reset; /* write does not guarantee ECONNRESET */

	void (* callback_check_sockaddr)(const struct sockaddr *sockaddr,
		socklen_t sockaddrlen, const char *callname, int addridx);
	void (* callback_cleanup)(void);
	void (* callback_xfer_peercred)(int sd); /* can be NULL */
	void (* callback_xfer_prepclient)(void); /* can be NULL */
	void (* callback_set_listen_opt)(int sd); /* can be NULL */
};

void test_abort_client_server(const struct socket_test_info *info,
	int abort_type);
void test_bind(const struct socket_test_info *info);
void test_close(const struct socket_test_info *info);
void test_connect_close(const struct socket_test_info *info);
void test_connect_nb(const struct socket_test_info *info);
void test_dup(const struct socket_test_info *info);
void test_dup2(const struct socket_test_info *info);
void test_getsockname(const struct socket_test_info *info);
void test_intr(const struct socket_test_info *info);
void test_listen(const struct socket_test_info *info);
void test_listen_close(const struct socket_test_info *info);
void test_listen_close_nb(const struct socket_test_info *info);
void test_msg_dgram(const struct socket_test_info *info);
void test_nonblock(const struct socket_test_info *info);
void test_read(const struct socket_test_info *info);
void test_shutdown(const struct socket_test_info *info);
void test_simple_client_server(const struct socket_test_info *info, int type);
void test_sockopts(const struct socket_test_info *info);
void test_socket(const struct socket_test_info *info);
void test_write(const struct socket_test_info *info);
void test_xfer(const struct socket_test_info *info);
