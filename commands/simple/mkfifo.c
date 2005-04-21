/* mkfifo - Make FIFO special files		Author: V. Archer */

/* Copyright 1991 by Vincent Archer
 *	You may freely redistribute this software, in source or binary
 *	form, provided that you do not alter this copyright mention in any
 *	way.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <minix/minlib.h>
#include <stdio.h>

#define USR_MODES (S_ISUID|S_IRWXU)
#define GRP_MODES (S_ISGID|S_IRWXG)
#define EXE_MODES (S_IXUSR|S_IXGRP|S_IXOTH)
#ifdef S_ISVTX
#define ALL_MODES (USR_MODES|GRP_MODES|S_IRWXO|S_ISVTX)
#else
#define ALL_MODES (USR_MODES|GRP_MODES|S_IRWXO)
#endif
#define DEFAULT_MODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)


/* Global u_mask needed in changemode.h */
mode_t u_mask;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(mode_t parsemode, (char *symbolic, Mode_t oldmode));
_PROTOTYPE(void usage, (void));

/* Parse a P1003.2 4.7.7-conformant symbolic mode. */
mode_t parsemode(symbolic, oldmode)
char *symbolic;
mode_t oldmode;
{
  mode_t who, mask, newmode, tmpmask;
  char action;

  newmode = oldmode & ALL_MODES;
  while (*symbolic) {
	who = 0;
	for (; *symbolic; symbolic++) {
		if (*symbolic == 'a') {
			who |= ALL_MODES;
			continue;
		}
		if (*symbolic == 'u') {
			who |= USR_MODES;
			continue;
		}
		if (*symbolic == 'g') {
			who |= GRP_MODES;
			continue;
		}
		if (*symbolic == 'o') {
			who |= S_IRWXO;
			continue;
		}
		break;
	}
	if (!*symbolic || *symbolic == ',') usage();
	while (*symbolic) {
		if (*symbolic == ',') break;
		switch (*symbolic) {
		    default:
			usage();
		    case '+':
		    case '-':
		    case '=':	action = *symbolic++;
		}
		mask = 0;
		for (; *symbolic; symbolic++) {
			if (*symbolic == 'u') {
				tmpmask = newmode & S_IRWXU;
				mask |= tmpmask | (tmpmask << 3) | (tmpmask << 6);
				symbolic++;
				break;
			}
			if (*symbolic == 'g') {
				tmpmask = newmode & S_IRWXG;
				mask |= tmpmask | (tmpmask >> 3) | (tmpmask << 3);
				symbolic++;
				break;
			}
			if (*symbolic == 'o') {
				tmpmask = newmode & S_IRWXO;
				mask |= tmpmask | (tmpmask >> 3) | (tmpmask >> 6);
				symbolic++;
				break;
			}
			if (*symbolic == 'r') {
				mask |= S_IRUSR | S_IRGRP | S_IROTH;
				continue;
			}
			if (*symbolic == 'w') {
				mask |= S_IWUSR | S_IWGRP | S_IWOTH;
				continue;
			}
			if (*symbolic == 'x') {
				mask |= EXE_MODES;
				continue;
			}
			if (*symbolic == 's') {
				mask |= S_ISUID | S_ISGID;
				continue;
			}
			if (*symbolic == 'X') {
				if (S_ISDIR(oldmode) || (oldmode & EXE_MODES))
					mask |= EXE_MODES;
				continue;
			}
#ifdef S_ISVTX
			if (*symbolic == 't') {
				mask |= S_ISVTX;
				who |= S_ISVTX;
				continue;
			}
#endif
			break;
		}
		switch (action) {
		    case '=':
			if (who)
				newmode &= ~who;
			else
				newmode = 0;
		    case '+':
			if (who)
				newmode |= who & mask;
			else
				newmode |= mask & (~u_mask);
			break;
		    case '-':
			if (who)
				newmode &= ~(who & mask);
			else
				newmode &= ~mask | u_mask;
		}
	}
	if (*symbolic) symbolic++;
  }
  return(newmode);
}


/* Main module. Since only one option (-m mode) is allowed, there's no need
 * to include the whole getopt() stuff.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  int errors = 0;
  char *symbolic;

  if (argc > 2 && *argv[1] == '-' && strcmp(argv[1], "-m") != 0) usage();
  argc--;
  argv++;
  if (argc && strncmp(*argv, "-m", (size_t) 2) == 0) {
	argc--;
	if ((argv[0])[2])
		symbolic = (*argv++) + 2;
	else {
		if (!argc--) usage();
		argv++;
		symbolic = *argv++;
	}
	u_mask = umask(0);
	umask(u_mask);
  } else
	symbolic = (char *) 0;

  if (!argc) usage();
  for (; argc--; argv++)
	if (mkfifo(*argv, DEFAULT_MODE)) {
		perror(*argv);
		errors = 1;
	} else if (symbolic && chmod(*argv, parsemode(symbolic, DEFAULT_MODE))) {
		unlink(*argv);
		perror(*argv);
		errors = 1;
	}
  return(errors);
}


/* Posix command prototype. */
void usage()
{
  std_err("Usage: mkfifo [-m mode] file...\n");
  exit(1);
}
