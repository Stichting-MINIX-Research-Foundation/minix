/*  uname - print system name			Author: Earl Chew */

/* Print the following system information as returned by the uname()
 * function:
 *
 *	system name		Minix
 *	node name		waddles
 *	release name		1.5
 *	version			10
 *	machine name		i86
 *	arch			i86	(Minix specific)
 */

#include <sys/types.h>
#include <sys/utsname.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Define the uname components. */
#define ALL	 ((unsigned) 0x1F)
#define SYSNAME  ((unsigned) 0x01)
#define NODENAME ((unsigned) 0x02)
#define RELEASE  ((unsigned) 0x04)
#define U_MACHINE  ((unsigned) 0x10)
#define ARCH     ((unsigned) 0x20)

int main(int argc, char **argv );
void print(int fd, ... );
void usage(void );

#ifdef __STDC__
void print(int fd, ...)
#else
void print(fd)
int fd;
#endif
{
/* Print a sequence of strings onto the named channel. */
  va_list argp;
  char *p;

  va_start(argp, fd);
  while (1) {
	p = va_arg(argp, char *);
	if (p == (char *) NULL) break;
	write(fd, p, strlen(p));
  }
  va_end(argp);
}

char *name;

void usage()
{
  print(STDERR_FILENO, "Usage: ", name, " -snrvmpa\n", (char *) NULL);
  exit(EXIT_FAILURE);
}

int main(argc, argv)
int argc;
char **argv;
{
  int info;
  char *p;
  struct utsname un;

  name = strrchr(argv[0], '/');
  if (name == NULL) name = argv[0]; else name++;

  for (info = 0; argc > 1; argc--, argv++) {
  	if (argv[1][0] == '-') {
  		for (p = &argv[1][1]; *p; p++) {
  			switch (*p) {
				case 'a': info |= ALL;      break;
				case 'm': info |= U_MACHINE;  break;
				case 'n': info |= NODENAME; break;
				case 'r': info |= RELEASE;  break;
				case 's': info |= SYSNAME;  break;
				case 'v': info |= RELEASE;  break;
				case 'p': info |= ARCH;     break;
				default: usage();
  			}
		}
	} else {
		usage();
	}
  }

  if (info == 0) info = strcmp(name, "arch") == 0 ? ARCH : SYSNAME;

  if (uname(&un) != 0) {
	print(STDERR_FILENO, "unable to determine uname values\n", (char *) NULL);
	exit(EXIT_FAILURE);
  }

  if ((info & SYSNAME) != 0)
	print(STDOUT_FILENO, un.sysname, (char *) NULL);
  if ((info & NODENAME) != 0) {
	if ((info & (SYSNAME)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.nodename, (char *) NULL);
  }
  if ((info & RELEASE) != 0) {
	if ((info & (SYSNAME|NODENAME)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.release, (char *) NULL);
	print(STDOUT_FILENO, ".", (char *) NULL);
	print(STDOUT_FILENO, un.version, (char *) NULL);
  }
  if ((info & U_MACHINE) != 0) {
	if ((info & (SYSNAME|NODENAME|RELEASE)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.machine, (char *) NULL);
  }
  if ((info & ARCH) != 0) {
	if ((info & (SYSNAME|NODENAME|RELEASE|U_MACHINE)) != 0)
		print(STDOUT_FILENO, " ", (char *) NULL);
	print(STDOUT_FILENO, un.arch, (char *) NULL);
  }
  print(STDOUT_FILENO, "\n", (char *) NULL);
  return EXIT_SUCCESS;
}
