/* cat - concatenates files  		Author: Andy Tanenbaum */

/* 30 March 1990 - Slightly modified for efficiency by Norbert Schlenker. */
/* 23 March 2002 - Proper error messages by Kees J. Bot. */


#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <minix/minlib.h>
#include <stdio.h>

#define CHUNK_SIZE	(2048 * sizeof(char *))

static int unbuffered;
static char ibuf[CHUNK_SIZE];
static char obuf[CHUNK_SIZE];
static char *op = obuf;

int main(int argc, char **argv);
static void copyout(char *file, int fd);
static void output(char *buf, size_t count);
static void report(char *label);
static void fatal(char *label);

static char STDIN[] = "standard input";
static char STDOUT[] = "standard output";

static int excode = 0;

int main(int argc, char *argv[])
{
  int i, fd;

  i = 1;
  while (i < argc && argv[i][0] == '-') {
	char *opt = argv[i] + 1;

	if (opt[0] == 0) break;				/* - */
	i++;
	if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

	while (*opt != 0) switch (*opt++) {
	case 'u':
		unbuffered = 1;
		break;
	default:
		std_err("Usage: cat [-u] [file ...]\n");
		exit(1);
	}
  }

  if (i >= argc) {
	copyout(STDIN, STDIN_FILENO);
  } else {
	while (i < argc) {
		char *file = argv[i++];

		if (file[0] == '-' && file[1] == 0) {
			copyout(STDIN, STDIN_FILENO);
		} else {
			fd = open(file, O_RDONLY);
			if (fd < 0) {
				report(file);
			} else {
				copyout(file, fd);
				close(fd);
			}
		}
	}
  }
  output(obuf, (op - obuf));
  return(excode);
}

static void copyout(char *file, int fd)
{
  int n;

  while (1) {
	n = read(fd, ibuf, CHUNK_SIZE);
	if (n < 0) fatal(file);
	if (n == 0) return;
	if (unbuffered || (op == obuf && n == CHUNK_SIZE)) {
		output(ibuf, n);
	} else {
		int bytes_left;

		bytes_left = &obuf[CHUNK_SIZE] - op;
		if (n <= bytes_left) {
			memcpy(op, ibuf, (size_t)n);
			op += n;
		} else {
			memcpy(op, ibuf, (size_t)bytes_left);
			output(obuf, CHUNK_SIZE);
			n -= bytes_left;
			memcpy(obuf, ibuf + bytes_left, (size_t)n);
			op = obuf + n;
		}
	}
  }
}

static void output(char *buf, size_t count)
{
  ssize_t n;

  while (count > 0) {
	n = write(STDOUT_FILENO, buf, count);
	if (n <= 0) {
		if (n < 0) fatal(STDOUT);
		std_err("cat: standard output: EOF\n");
		exit(1);
	}
	buf += n;
	count -= n;
  }
}

static void report(char *label)
{
  int e = errno;
  std_err("cat: ");
  std_err(label);
  std_err(": ");
  std_err(strerror(e));
  std_err("\n");
  excode = 1;
}

static void fatal(char *label)
{
  report(label);
  exit(1);
}
