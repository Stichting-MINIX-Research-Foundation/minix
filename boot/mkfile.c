/*	mkfile 1.0 - create a file under DOS for use as a Minix "disk".
 *							Author: Kees J. Bot
 *								9 May 1998
 */
#define nil 0
#include <sys/types.h>
#include <string.h>
#include <limits.h>

/* Stuff normally found in <unistd.h>, <errno.h>, etc. */
extern int errno;
int creat(const char *file, int mode);
int open(const char *file, int oflag);
off_t lseek(int fd, off_t offset, int whence);
ssize_t write(int fd, const char *buf, size_t len);
void exit(int status);
int printf(const char *fmt, ...);

#define O_WRONLY	1
#define SEEK_SET	0
#define SEEK_END	2

/* Kernel printf requires a putk() function. */
int putk(int c)
{
	char ch = c;

	if (c == 0) return;
	if (c == '\n') putk('\r');
	(void) write(2, &ch, 1);
}

static void usage(void)
{
	printf("Usage: mkfile <size>[gmk] <file>\n"
		"(Example sizes, all 50 meg: 52428800, 51200k, 50m)\n");
	exit(1);
}

char *strerror(int err)
/* Translate some DOS error numbers to text. */
{
	static struct errlist {
		int	err;
		char	*what;
	} errlist[] = {
		{  0, "No error" },
		{  1, "Function number invalid" },
		{  2, "File not found" },
		{  3, "Path not found" },
		{  4, "Too many open files" },
		{  5, "Access denied" },
		{  6, "Invalid handle" },
		{ 12, "Access code invalid" },
		{ 39, "Insufficient disk space" },
	};
	struct errlist *ep;
	static char unknown[]= "Error 65535";
	unsigned e;
	char *p;

	for (ep= errlist; ep < errlist + sizeof(errlist)/sizeof(errlist[0]);
									ep++) {
		if (ep->err == err) return ep->what;
	}
	p= unknown + sizeof(unknown) - 1;
	e= err;
	do *--p= '0' + (e % 10); while ((e /= 10) > 0);
	strcpy(unknown + 6, p);
	return unknown;
}

int main(int argc, char **argv)
{
	int i;
	static char buf[512];
	unsigned long size, mul;
	off_t offset;
	char *cp;
	int fd;
	char *file;

	if (argc != 3) usage();

	cp= argv[1];
	size= 0;
	while ((unsigned) (*cp - '0') < 10) {
		unsigned d= *cp++ - '0';
		if (size <= (ULONG_MAX-9) / 10) {
			size= size * 10 + d;
		} else {
			size= ULONG_MAX;
		}
	}
	if (cp == argv[1]) usage();
	while (*cp != 0) {
		mul = 1;
		switch (*cp++) {
		case 'G':
		case 'g':	mul *= 1024;
		case 'M':
		case 'm':	mul *= 1024;
		case 'K':
		case 'k':	mul *= 1024;
		case 'B':
		case 'b':	break;
		default:	usage();
		}
		if (size <= ULONG_MAX / mul) {
			size *= mul;
		} else {
			size= ULONG_MAX;
		}
	}

	if (size > 1024L*1024*1024) {
		printf("mkfile: A file size over 1G is a bit too much\n");
		exit(1);
	}

	/* Open existing file, or create a new file. */
	file= argv[2];
	if ((fd= open(file, O_WRONLY)) < 0) {
		if (errno == 2) {
			fd= creat(file, 0666);
		}
	}
	if (fd < 0) {
		printf("mkfile: Can't open %s: %s\n", file, strerror(errno));
		exit(1);
	}

	/* How big is the file now? */
	if ((offset= lseek(fd, 0, SEEK_END)) == -1) {
		printf("mkfile: Can't seek in %s: %s\n", file, strerror(errno));
		exit(1);
	}

	if (offset == 0 && size == 0) exit(0);	/* Huh? */

	/* Write the first bit if the file is zero length.  This is necessary
	 * to circumvent a DOS bug by extending a new file by lseek.  We also
	 * want to make sure there are zeros in the first sector.
	 */
	if (offset == 0) {
		if (write(fd, buf, sizeof(buf)) == -1) {
			printf("mkfile: Can't write to %s: %s\n",
				file, strerror(errno));
			exit(1);
		}
	}

	/* Seek to the required size and write 0 bytes to extend/truncate the
	 * file to that size.
	 */
	if (lseek(fd, size, SEEK_SET) == -1) {
		printf("mkfile: Can't seek in %s: %s\n", file, strerror(errno));
		exit(1);
	}
	if (write(fd, buf, 0) == -1) {
		printf("mkfile: Can't write to %s: %s\n",
			file, strerror(errno));
		exit(1);
	}

	/* Did the file become the required size? */
	if ((offset= lseek(fd, 0, SEEK_END)) == -1) {
		printf("mkfile: Can't seek in %s: %s\n", file, strerror(errno));
		exit(1);
	}
	if (offset != size) {
		printf("mkfile: Failed to extend %s.  Disk full?\n", file);
		exit(1);
	}
	return 0;
}

/*
 * $PchId: mkfile.c,v 1.4 2000/08/13 22:06:40 philip Exp $
 */
