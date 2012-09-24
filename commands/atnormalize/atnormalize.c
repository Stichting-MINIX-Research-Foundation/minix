#include <sys/types.h>
#include <stdio.h>
#include <termcap.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/partition.h>
#include <machine/partition.h>
#include <termios.h>
#include <stdarg.h>

int main(void)
{
	int v, d;
	char name[20];

	for(d = 0; d < 4; d++) {
		int device;
		sprintf(name, "/dev/c0d%d", d);
		if((device=open(name, O_RDONLY)) >= 0) {
			v = 0;
			ioctl(device, DIOCTIMEOUT, &v);
			close(device);
		}
	}

	return 0;
}
