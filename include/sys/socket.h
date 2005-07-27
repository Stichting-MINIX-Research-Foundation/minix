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

#define SO_ERROR	0x1007

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
	char		sa_data[1];
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
_PROTOTYPE( int listen, (int _socket, int _backlog)			);
_PROTOTYPE( ssize_t recvfrom, (int _socket, void *_RESTRICT _buffer,
	size_t _length, int _flags, struct sockaddr *_RESTRICT _address,
				socklen_t *_RESTRICT _address_len)	);
_PROTOTYPE( ssize_t sendto, (int _socket, const void *_message,
	size_t _length, int _flags, const struct sockaddr *_dest_addr,
						socklen_t _dest_len)	);
_PROTOTYPE( int shutdown, (int _socket, int _how)			);
_PROTOTYPE( int socket, (int _domain, int _type, int _protocol)		);

#endif /* SYS_SOCKET_H */
