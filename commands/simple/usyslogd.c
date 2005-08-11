/* Microsyslogd that does basic syslogging.
 */

#include <stdio.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

char *nodename;

void logline(FILE *outfp, char *proc, char *line)
{
	time_t now;
	struct tm *tm;
	char *d, *s;
	time(&now);
	tm = localtime(&now);
	d=asctime(tm);

	/* Trim off year and newline. */
	if((s=strrchr(d, ' ')))
		*s = '\0';
	if(s=strchr(d, ' ')) d = s+1;
	fprintf(outfp, "%s %s: %s\n", d, nodename, line);
}

void copy(int in_fd, FILE *outfp)
{
	static char linebuf[5*1024];
	int l, acc = 0;
	while((l=read(in_fd, linebuf, sizeof(linebuf)-2)) > 0) {
		char *b, *eol;
		int i;
		acc += l;
		for(i = 0; i < l; i++)
			if(linebuf[i] == '\0')
				linebuf[i] = ' ';
		if(linebuf[l-1] == '\n') l--;
		linebuf[l] = '\n';
		linebuf[l+1] = '\0';
		b = linebuf;
		while(eol = strchr(b, '\n')) {
			*eol = '\0';
			logline(outfp, "kernel", b);
			b = eol+1;
		}
	}

	/* Nothing sensible happened? Avoid busy-looping. */
	if(!acc) sleep(1);

	return;
}

int
main(int argc, char *argv[])
{
	int config_fd, klog_fd, n, maxfd;
	char *nn;
	FILE *logfp;
	struct utsname utsname;

	if(uname(&utsname) < 0) {
		perror("uname");
		return 1;
	}

	nodename = utsname.nodename;
	if((nn=strchr(nodename, '.')))
		*nn = '\0';

	if((klog_fd = open("/dev/klog", O_NONBLOCK | O_RDONLY)) < 0) {
		perror("/dev/klog");
		return 1;
	}

	if(!(logfp = fopen("/usr/log/messages", "a"))) {
		return 1;
	}

	maxfd = klog_fd;

	while(1) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(klog_fd, &fds);
		n = select(maxfd+1, &fds, NULL, NULL, NULL);
		if(n <= 0) {
			sleep(1);
			continue;
		}
		if(FD_ISSET(klog_fd, &fds)) {
			copy(klog_fd, logfp);
		}
		fflush(logfp);
		sync();
	}

	return 0;
}

