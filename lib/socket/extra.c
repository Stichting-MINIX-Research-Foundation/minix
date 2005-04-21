
/* bsd-socket(2)-lookalike */

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <net/gen/socket.h>
#include <net/gen/emu.h>
#include <net/gen/tcp.h>
#include <net/gen/in.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>

ssize_t
recv(int s, void *buf, size_t len, int flags)
{
	return read(s, buf, len);
}

ssize_t
send(int s, void *buf, size_t len, int flags)
{
	return write(s, buf, len);
}

