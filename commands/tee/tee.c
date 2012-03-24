/* tee - pipe fitting			Author: Paul Polderman */

#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <minix/minlib.h>

#define	MAXFD	18
#define CHUNK_SIZE	4096

int fd[MAXFD];

int main(int argc, char **argv);

int main(argc, argv)
int argc;
char **argv;
{
  char iflag = 0, aflag = 0;
  char buf[CHUNK_SIZE];
  int i, s, n;

  argv++;
  --argc;
  while (argc > 0 && argv[0][0] == '-') {
	switch (argv[0][1]) {
	    case 'i':		/* Interrupt turned off. */
		iflag++;
		break;
	    case 'a':		/* Append to outputfile(s), instead of
			 * overwriting them. */
		aflag++;
		break;
	    default:
		std_err("Usage: tee [-i] [-a] [files].\n");
		exit(1);
	}
	argv++;
	--argc;
  }
  fd[0] = 1;			/* Always output to stdout. */
  for (s = 1; s < MAXFD && argc > 0; --argc, argv++, s++) {
	if (aflag && (fd[s] = open(*argv, O_RDWR)) >= 0) {
		lseek(fd[s], 0L, SEEK_END);
		continue;
	} else {
		if ((fd[s] = creat(*argv, 0666)) >= 0) continue;
	}
	std_err("Cannot open output file: ");
	std_err(*argv);
	std_err("\n");
	exit(2);
  }

  if (iflag) signal(SIGINT, SIG_IGN);

  while ((n = read(0, buf, CHUNK_SIZE)) > 0) {
	for (i = 0; i < s; i++) write(fd[i], buf, n);
  }

  for (i = 0; i < s; i++)	/* Close all fd's */
	close(fd[i]);
  return(0);
}
