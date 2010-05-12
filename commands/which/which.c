/* which - search paths for executable */

#define DELIMITER ':'

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

_PROTOTYPE(int main, (int argc, char **argv));

int main(ac, av)
int ac;
char **av;
{
  char *path, *cp;
  char buf[400];
  char prog[400];
  char patbuf[512];
  int quit, none;
  int excode = 0;

  if (ac < 2) {
	fprintf(stderr, "Usage: %s cmd [cmd, ..]\n", *av);
	exit(1);
  }
  av[ac] = 0;
  for (av++; *av; av++) {

	quit = 0;
	none = 1;
	if ((path = getenv("PATH")) == NULL) {
		fprintf(stderr, "Null path.\n");
		exit(0);
	}
	strcpy(patbuf, path);
	path = patbuf;
	cp = path;

	while (1) {
		cp = strchr(path, DELIMITER);
		if (cp == NULL)
			quit++;
		else
			*cp = '\0';

		if (strcmp(path, "") == 0 && quit == 0) {
			sprintf(buf, "%s./%s", path, *av);
		} else
			sprintf(buf, "%s/%s", path, *av);

		/* Fprintf(stderr,"Trying %s, path %s\n",buf,path); */

		path = ++cp;

		if (access(buf, 1) == 0) {
			printf("%s\n", buf);
			none = 0;
		}
		sprintf(prog, "%s.%s", buf, "prg");
		if (access(prog, 1) == 0) {
			printf("%s\n", prog);
			none = 0;
		}
		sprintf(prog, "%s.%s", buf, "ttp");
		if (access(prog, 1) == 0) {
			printf("%s\n", prog);
			none = 0;
		}
		sprintf(prog, "%s.%s", buf, "tos");
		if (access(prog, 1) == 0) {
			printf("%s\n", prog);
			none = 0;
		}
		if (quit) {
			if (none) {
				fprintf(stderr, "No %s in %s\n", *av, getenv("PATH"));
				excode = 1;
			}
			break;
		}
	}
  }
  return(excode);
}
