#include <stdlib.h>
#include <sys/socket.h>

ssize_t recv(int socket, void *buffer, size_t length, int flags)
{
	return recvfrom(socket, buffer, length, flags, NULL, NULL);
}

