/*
type.h

Copyright 1995 Philip Homburg
*/

#ifndef INET_TYPE_H
#define INET_TYPE_H

typedef struct acc *(*get_userdata_t) ARGS(( int fd, size_t offset,
	size_t count, int for_ioctl ));
typedef int (*put_userdata_t) ARGS(( int fd, size_t offset,
	struct acc *data, int for_ioctl ));
typedef void (*put_pkt_t) ARGS(( int fd, struct acc *data, size_t datalen ));

#endif /* INET_TYPE_H */

/*
 * $PchId: type.h,v 1.5 1995/11/21 06:51:58 philip Exp $
 */
