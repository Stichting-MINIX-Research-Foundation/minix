/*
emu.h - bsd sockets lookalike interface
*/

#ifndef __EMU_H__
#define __EMU_H__

typedef int socklen_t;
typedef int sa_family_t;
typedef int in_port_t;
typedef unsigned long sin_addr_t;
typedef unsigned long in_addr_t;

/* generic socket address */
struct sockaddr {
	unsigned char	sa_len;		/* total address size */
	sa_family_t	sa_family;	/* address type (family) */
	char		sa_data[14];	/* family-dependent addr (may be longer) */
};

struct in_addr {
	in_addr_t	s_addr;
};

/* internet-domain (AF_INET) socket address */
struct sockaddr_in {
	unsigned char	sin_len;
	sa_family_t	sin_family;
	in_port_t	sin_port;
	struct in_addr	sin_addr;
	char		sin_zero[8];
};

/* type argument to socket() */
#define SOCK_STREAM	2
#define SOCK_DGRAM	3

/* protocol argument to socket() */
#define IPPROTO_ICMP	1
#define IPPROTO_TCP	6
#define IPPROTO_UDP	17

/* 2nd args to shutdown() */
#define SHUT_RD		0
#define SHUT_WR		1
#define SHUT_RDWR	2

/* bsd-lookalike functions */
int socket(int, int, int);
int bind(int, struct sockaddr *, socklen_t);
int listen(int, int);
int shutdown(int, int);
int connect(int, struct sockaddr *, socklen_t);

int recv(int s, void *buf, size_t len, int flags);
int send(int s, void *buf, size_t len, int flags);

#endif /* __EMU_H__ */

