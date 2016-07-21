#ifndef MINIX_TEST_SOCKLIB_H
#define MINIX_TEST_SOCKLIB_H

enum state {
	S_NEW,
	S_N_SHUT_R,
	S_N_SHUT_W,
	S_N_SHUT_RW,
	S_BOUND,
	S_LISTENING,
	S_L_SHUT_R,
	S_L_SHUT_W,
	S_L_SHUT_RW,
	S_CONNECTING,
	S_C_SHUT_R,
	S_C_SHUT_W,
	S_C_SHUT_RW,
	S_CONNECTED,
	S_ACCEPTED,
	S_SHUT_R,
	S_SHUT_W,
	S_SHUT_RW,
	S_RSHUT_R,
	S_RSHUT_W,
	S_RSHUT_RW,
	S_SHUT2_R,
	S_SHUT2_W,
	S_SHUT2_RW,
	S_PRE_EOF,
	S_AT_EOF,
	S_POST_EOF,
	S_PRE_SHUT_R,
	S_EOF_SHUT_R,
	S_POST_SHUT_R,
	S_PRE_SHUT_W,
	S_EOF_SHUT_W,
	S_POST_SHUT_W,
	S_PRE_SHUT_RW,
	S_EOF_SHUT_RW,
	S_POST_SHUT_RW,
	S_PRE_RESET,
	S_AT_RESET,
	S_POST_RESET,
	S_FAILED,
	S_POST_FAILED,
	S_MAX
};

enum call {
	C_ACCEPT,
	C_BIND,
	C_CONNECT,
	C_GETPEERNAME,
	C_GETSOCKNAME,
	C_GETSOCKOPT_ERR,
	C_GETSOCKOPT_KA,
	C_GETSOCKOPT_RB,
	C_IOCTL_NREAD,
	C_LISTEN,
	C_RECV,
	C_RECVFROM,
	C_SEND,
	C_SENDTO,
	C_SELECT_R,
	C_SELECT_W,
	C_SELECT_X,
	C_SETSOCKOPT_BC,
	C_SETSOCKOPT_KA,
	C_SETSOCKOPT_L,
	C_SETSOCKOPT_RA,
	C_SHUTDOWN_R,
	C_SHUTDOWN_RW,
	C_SHUTDOWN_W,
	C_MAX
};

int socklib_sweep_call(enum call call, int fd, struct sockaddr * local_addr,
	struct sockaddr * remote_addr, socklen_t addr_len);
void socklib_sweep(int domain, int type, int protocol,
	const enum state * states, unsigned int nstates, const int * results,
	int (* proc)(int domain, int type, int protocol, enum state,
	enum call));

void socklib_large_transfers(int fd[2]);
void socklib_producer_consumer(int fd[2]);
void socklib_stream_recv(int (* socket_pair)(int, int, int, int *), int domain,
	int type, int (* break_recv)(int, const char *, size_t));

#endif /* !MINIX_TEST_SOCKLIB_H */
