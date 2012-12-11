/* cmp - compare two files		Author: Kees J. Bot.  */

#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

void fatal(char *label);
int cmp(int fd1, int fd2);
void Usage(void);
int main(int argc, char **argv);

#define BLOCK	4096

static int loud = 0, silent = 0;
static char *name1, *name2;

int main(argc, argv)
int argc;
char **argv;
{
  int fd1, fd2;

  /* Process the '-l' or '-s' option. */
  while (argc > 1 && argv[1][0] == '-' && argv[1][1] != 0) {
  	if (argv[1][2] != 0) Usage();

  	switch (argv[1][1]) {
  	case '-':
  		/* '--': no-op option. */
  		break;
  	case 'l':
		loud = 1;
		break;
	case 's':
		silent = 1;
		break;
	default:
		Usage();
	}
	argc--;
	argv++;
  }
  if (argc != 3) Usage();

  /* Open the first file, '-' means standard input. */
  if (argv[1][0] == '-' && argv[1][1] == 0) {
	name1 = "stdin";
	fd1 = 0;
  } else {
	name1 = argv[1];
	if ((fd1 = open(name1, 0)) < 0) fatal(name1);
  }

  /* Second file likewise. */
  if (argv[2][0] == '-' && argv[2][1] == 0) {
	name2 = "stdin";
	fd2 = 0;
  } else {
	name2 = argv[2];
	if ((fd2 = open(name2, 0)) < 0) fatal(name2);
  }

  exit(cmp(fd1, fd2));
}

int cmp(fd1, fd2)
int fd1, fd2;
{
  static char buf1[BLOCK], buf2[BLOCK];
  int n1 = 0, n2 = 0, i1 = 0, i2 = 0, c1, c2;
  off_t pos = 0, line = 1;
  int eof = 0, differ = 0;

  for (;;) {
	if (i1 == n1) {
		pos += n1;

		if ((n1 = read(fd1, buf1, sizeof(buf1))) <= 0) {
			if (n1 < 0) fatal(name1);
			eof |= 1;
		}
		i1 = 0;
	}
	if (i2 == n2) {
		if ((n2 = read(fd2, buf2, sizeof(buf2))) <= 0) {
			if (n2 < 0) fatal(name2);
			eof |= 2;
		}
		i2 = 0;
	}
	if (eof != 0) break;

	c1 = buf1[i1++];
	c2 = buf2[i2++];

	if (c1 != c2) {
		if (!loud) {
			if (!silent) {
				printf("%s %s differ: char %d, line %d\n",
				       name1, name2, pos + i1, line);
			}
			return(1);
		}
		printf("%10d %3o %3o\n", pos + i1, c1 & 0xFF, c2 & 0xFF);
		differ = 1;
	}
	if (c1 == '\n') line++;
  }
  if (eof == (1 | 2)) return(differ);
  if (!silent) fprintf(stderr, "cmp: EOF on %s\n", eof == 1 ? name1 : name2);
  return(1);
}

void fatal(label)
char *label;
{
  if (!silent) fprintf(stderr, "cmp: %s: %s\n", label, strerror(errno));
  exit(2);
}

void Usage()
{
  fprintf(stderr, "Usage: cmp [-l | -s] file1 file2\n");
  exit(2);
}
