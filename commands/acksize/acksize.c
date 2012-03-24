/* size - tell size of an object file		Author: Andy Tanenbaum */

#include <sys/types.h>
#include <fcntl.h>
#include <a.out.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int heading;			/* set when heading printed */
int error;

int main(int argc, char **argv);
void size(char *name);

int main(argc, argv)
int argc;
char *argv[];
{
  int i;

  if (argc == 1) {
	size("a.out");
	exit(error);
  }
  for (i = 1; i < argc; i++) size(argv[i]);
  return(error);
}



void size(name)
char *name;
{
  int fd, separate;
  long dynam, allmem;
  struct exec exec;

  if ((fd = open(name, O_RDONLY)) < 0) {
	fprintf(stderr, "size: can't open %s\n", name);
	error = 1;
	return;
  }
  if (read(fd, (char *)&exec, sizeof(struct exec)) != sizeof(struct exec)) {
	fprintf(stderr, "size: %s: header too short\n", name);
	error = 1;
	close(fd);
	return;
  }
  if (BADMAG(exec)) {
	fprintf(stderr, "size: %s not an object file\n", name);
	error = 1;
	close(fd);
	return;
  }
  separate = (exec.a_flags & A_SEP ? 1 : 0);
  dynam = exec.a_total - exec.a_text - exec.a_data - exec.a_bss;
  if (separate) dynam += exec.a_text;
  allmem = (separate ? exec.a_total + exec.a_text : exec.a_total);
  if (heading++ == 0) printf("   text    data     bss    stack   memory\n");
  printf("%7ld %7ld %7ld %8ld %8ld  %s\n",
         exec.a_text, exec.a_data, exec.a_bss, dynam, allmem, name);
  close(fd);
}
