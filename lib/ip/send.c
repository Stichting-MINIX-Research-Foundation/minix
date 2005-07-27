#include <stdlib.h>
#include <sys/socket.h>

ssize_t send(int socket, const void *buffer, size_t length, int flags)
{
	struct sockaddr sa;

	sa.sa_family= AF_UNSPEC;
	return sendto(socket, buffer, length, flags, &sa, sizeof(sa));
}

