/*
sys/socket.h
*/

#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H

/* Can we include <stdint.h> here or do we need an additional header that is
 * safe to include?
 */
#include <stdint.h>

/* Open Group Base Specifications Issue 6 (not complete) */
#include <net/gen/socket.h>

#define SOCK_STREAM	1
#define SOCK_DGRAM	2
#define SOCK_RAW	3
#define SOCK_RDM	4
#define SOCK_SEQPACKET	5

#define SOL_SOCKET	0xFFFF

#define SO_DEBUG	0x0001
#define SO_REUSEADDR	0x0004
#define SO_KEEPALIVE	0x0008
#define SO_TYPE	0x0010	/* get socket type, SOCK_STREAM or SOCK_DGRAM */

#define SO_SNDBUF	0x1001	/* send buffer size */
#define SO_RCVBUF	0x1002	/* receive buffer size */
#define SO_ERROR	0x1007	/* get and clear error status */

/* The how argument to shutdown */
#define SHUT_RD		0	/* No further reads */
#define SHUT_WR		1	/* No further writes */
#define SHUT_RDWR	2	/* No further reads and writes */

#ifndef _SA_FAMILY_T
#define _SA_FAMILY_T
typedef uint8_t		sa_family_t;
#endif /* _SA_FAMILY_T */

typedef int32_t socklen_t;

struct sockaddr
{
	sa_family_t	sa_family;
	char		sa_data[8];	/* Big enough for sockaddr_in */
};

_PROTOTYPE( int accept, (int _socket,
				struct sockaddr *_RESTRICT _address,
				socklen_t *_RESTRICT _address_len)	);
_PROTOTYPE( int bind, (int _socket, const struct sockaddr *_address,
						socklen_t _address_len)	);
_PROTOTYPE( int connect, (int _socket, const struct sockaddr *_address,
						socklen_t _address_len)	);
_PROTOTYPE( int getpeername, (int _socket,
				struct sockaddr *_RESTRICT _address,
				socklen_t *_RESTRICT _address_len)	);
_PROTOTYPE( int getsockname, (int _socket,
				struct sockaddr *_RESTRICT _address,
				socklen_t *_RESTRICT _address_len)	);
_PROTOTYPE( int setsockopt,(int _socket, int _level, int _option_name,
		const void *_option_value, socklen_t _option_len)	);
_PROTOTYPE( int getsockopt, (int _socket, int _level, int _option_name,
        void *_RESTRICT _option_value, socklen_t *_RESTRICT _option_len));
_PROTOTYPE( int listen, (int _socket, int _backlog)			);
_PROTOTYPE( ssize_t recv, (int _socket, void *_buffer, size_t _length,
							int _flags)	);
_PROTOTYPE( ssize_t recvfrom, (int _socket, void *_RESTRICT _buffer,
	size_t _length, int _flags, struct sockaddr *_RESTRICT _address,
				socklen_t *_RESTRICT _address_len)	);
_PROTOTYPE( ssize_t send, (int _socket, const void *_buffer,
					size_t _length, int _flags)	);
_PROTOTYPE( ssize_t sendto, (int _socket, const void *_message,
	size_t _length, int _flags, const struct sockaddr *_dest_addr,
						socklen_t _dest_len)	);
_PROTOTYPE( int shutdown, (int _socket, int _how)			);
_PROTOTYPE( int socket, (int _domain, int _type, int _protocol)		);

/* The following constants are often used in applications, but are not defined
 * by POSIX.
 */
#define PF_INET		AF_INET
#define PF_INET6	AF_INET6
#define PF_UNIX		AF_UNIX
#define PF_UNSPEC	AF_UNSPEC

/* based on http://tools.ietf.org/html/rfc2553 */
struct sockaddr_storage
{
	sa_family_t	__ss_family;
	char		__ss_pad1[6];
#ifdef __LONG_LONG_SUPPORTED
	int64_t		__ss_align;
#else
	int32_t		__ss_align[2];
#endif
	char		__ss_pad2[112];
};

#endif /* SYS_SOCKET_H */
