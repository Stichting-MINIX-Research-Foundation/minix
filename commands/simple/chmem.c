/* chmem - set total memory size for execution	Author: Andy Tanenbaum */

#include <minix/config.h>
#include <sys/types.h>
#include <a.out.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_8086     0x10000L	/* maximum allocation size for 8086 */
#define MAX_386	  0x7FFFFFFFL	/* etc */
#define MAX_68K   0x7FFFFFFFL
#define MAX_SPARC 0x20000000L	/* No more than 512MB on a SparcStation! */

char *progname;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void error, (char *s1, char *s2));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
/* The 8088 architecture does not make it possible to catch stacks that grow
 * big.  The only way to deal with this problem is to let the stack grow down
 * towards the data segment and the data segment grow up towards the stack.
 * Normally, a total of 64K is allocated for the two of them, but if the
 * programmer knows that a smaller amount is sufficient, he can change it
 * using chmem.
 *
 * chmem =4096 prog  sets the total space for stack + data growth to 4096
 * chmem +200  prog  increments the total space for stack + data growth by 200
 */

  char *p;
  int fd, separate;
  size_t s;
  long lsize, olddynam, newdynam, newtot, overflow;
  struct exec exec;
  char cpu;
  long max;

  progname = argv[0];
  if (argc < 3) usage();
  p = argv[1];
  if (*p != '=' && *p != '+' && *p != '-') usage();
  lsize = atol(p + 1);
  s = sizeof(struct exec);

  if (lsize < 0) {
	error(p + 1, "is negative");
	exit(1);
  }
  argc -= 1;
  argv += 1;

  while (--argc) {
	++argv;
	fd = open(*argv, O_RDWR);
	if (fd < 0) {
		error("can't open", *argv);
		continue;
	}
	if (read(fd, (char *) &exec, s) != s) {
		error("can't read header in", *argv);
		continue;
	}
	if (BADMAG(exec)) {
		error(*argv, "is not executable");
		continue;
	}
	separate = (exec.a_flags & A_SEP ? 1 : 0);
	cpu = exec.a_cpu;

#if (CHIP == M68000)
	if (cpu == A_I8086) cpu = A_M68K;
#endif

	switch (cpu) {
	    case A_I8086:	max = MAX_8086;	break;
	    case A_I80386:	max = MAX_386;	break;
	    case A_M68K:	max = MAX_68K;	break;
	    case A_SPARC:	max = MAX_SPARC;	break;
	    default:
		error("bad CPU type in", *argv);
		continue;
	}

	if (lsize > max) {
		error("size is too large for", *argv);
		continue;
	}
	olddynam = exec.a_total - exec.a_data - exec.a_bss;
	if (separate == 0) olddynam -= exec.a_text;

	if (*p == '=')
		newdynam = lsize;
	else if (*p == '+')
		newdynam = olddynam + lsize;
	else if (*p == '-')
		newdynam = olddynam - lsize;

	newtot = exec.a_data + exec.a_bss + newdynam;
	if (separate == 0) newtot += exec.a_text;
	overflow = (newtot > max ? newtot - max : 0);
	newdynam -= overflow;
	newtot -= overflow;
	exec.a_total = newtot;
	lseek(fd, (long) 0, SEEK_SET);
	if (write(fd, (char *) &exec, s) != s) {
		error("can't modify", *argv);
		continue;
	}
	printf("%s: Stack+malloc area changed from %ld to %ld bytes.\n",
	       *argv, olddynam, newdynam);
	close(fd);
  }
  return(0);
}

void error(s1, s2)
char *s1;
char *s2;
{
  fprintf(stderr, "%s: %s ", progname, s1);
  if (errno != 0)
	perror(s2);
  else
	fprintf(stderr, "%s\n", s2);
  errno = 0;
}

void usage()
{
  fprintf(stderr, "Usage: %s {=+-} amount file\n", progname);
  exit(1);
}
