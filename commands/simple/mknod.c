/* mknod - build a special file		Author: Andy Tanenbaum */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <minix/minlib.h>
#include <errno.h>
#include <stdio.h>

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void badcomm, (void));
_PROTOTYPE(void badfifo, (void));
_PROTOTYPE(void badchar, (void));
_PROTOTYPE(void badblock, (void));

int main(argc, argv)
int argc;
char *argv[];
{
/* Mknod name b/c major minor makes a node. */

  int mode, major, minor, dev;

  if (argc < 3) badcomm();
  if (*argv[2] != 'b' && *argv[2] != 'c' && *argv[2] != 'p') badcomm();
  if (*argv[2] == 'p' && argc != 3) badfifo();
  if (*argv[2] == 'c' && argc != 5) badchar();
  if (*argv[2] == 'b' && argc != 5) badblock();
  if (*argv[2] == 'p') {
	mode = 010666;
	dev = 0;
  } else {
	mode = (*argv[2] == 'b' ? 060666 : 020666);
	major = atoi(argv[3]);
	minor = atoi(argv[4]);
	if (major - 1 > 0xFE || minor > 0xFF) badcomm();
	dev = (major << 8) | minor;
  }
  if (mknod(argv[1], mode, dev) < 0) {
	int err = errno;
	std_err("mknod: ");
	errno = err;
	perror(argv[1]);
  }
  return(0);
}

void badcomm()
{
  std_err("Usage: mknod name b/c/p [major minor]\n");
  exit(1);
}

void badfifo()
{
  std_err("Usage: mknod name p\n");
  exit(1);
}

void badchar()
{
  std_err("Usage: mknod name c major minor\n");
  exit(1);
}

void badblock()
{
  std_err("Usage: mknod name b major minor\n");
  exit(1);
}
