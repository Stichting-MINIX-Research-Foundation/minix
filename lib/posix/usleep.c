/*
lib/posix/usleep.c
*/

#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

int usleep(useconds_t useconds)
{
	int r;
	struct timeval tv;

	tv.tv_sec= useconds/1000000;
	tv.tv_usec= useconds % 1000000;
	r= select(0, NULL, NULL, NULL, &tv);
	return r;
}
