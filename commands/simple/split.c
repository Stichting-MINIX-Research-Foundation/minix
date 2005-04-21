/* split - split a file			Author: Michiel Huisjes */

#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <minix/minlib.h>

#define CHUNK_SIZE	1024

int cut_line = 1000;
int infile;
char out_file[100];
char *suffix;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void split, (void));
_PROTOTYPE(int newfile, (void));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(void quit, (void));

int main(argc, argv)
int argc;
char **argv;
{
  unsigned short i;

  out_file[0] = 'x';
  infile = -1;

  if (argc > 4) usage();
  for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
		if (argv[i][1] >= '0' && argv[i][1] <= '9'
		    && cut_line == 1000)
			cut_line = atoi(argv[i]);
		else if (argv[i][1] == '\0' && infile == -1)
			infile = 0;
		else
			usage();
	} else if (infile == -1) {
		if ((infile = open(argv[i], O_RDONLY)) < 0) {
			std_err("Cannot open input file.\n");
			exit(1);
		}
	} else
		strcpy(out_file, argv[i]);
  }
  if (infile == -1) infile = 0;
  strcat(out_file, "aa");
  for (suffix = out_file; *suffix; suffix++);
  suffix--;

/* Appendix now points to last `a' of "aa". We have to decrement it by one */
  *suffix = 'a' - 1;
  split();
  return(0);
}

void split()
{
  char buf[CHUNK_SIZE];
  register char *index, *base;
  register int n;
  int fd;
  long lines = 0L;

  fd = -1;
  while ((n = read(infile, buf, CHUNK_SIZE)) > 0) {
	base = index = buf;
	while (--n >= 0) {
		if (*index++ == '\n')
			if (++lines % cut_line == 0) {
				if (fd == -1) fd = newfile();
				if (write(fd, base, (int) (index - base)) != (int) (index - base))
					quit();
				base = index;
				close(fd);
				fd = -1;
			}
	}
	if (index == base) continue;
	if (fd == -1) fd = newfile();
	if (write(fd, base, (int) (index - base)) != (int) (index - base))
		quit();
  }
}

int newfile()
{
  int fd;

  if (++*suffix > 'z') {	/* Increment letter */
	*suffix = 'a';		/* Reset last letter */
	++*(suffix - 1);	/* Previous letter must be incremented */
	/* E.g. was `filename.az' */
	/* Now `filename.ba' */
  }
  if ((fd = creat(out_file, 0644)) < 0) {
	std_err("Cannot create new file.\n");
	exit(2);
  }
  return fd;
}

void usage()
{
  std_err("Usage: split [-n] [file [name]].\n");
  exit(1);
}

void quit()
{
  std_err("split: write error\n");
  exit(1);
}
