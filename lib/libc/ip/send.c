#include <stdlib.h>
#include <sys/socket.h>

ssize_t send(int socket, const void *buffer, size_t length, int flags)
{
	return sendto(socket, buffer, length, flags, NULL, 0);
}

