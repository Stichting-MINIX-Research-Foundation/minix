/* mkdir - Make directories		Author: V. Archer */

/* Copyright 1991 by Vincent Archer
 *	You may freely redistribute this software, in source or binary
 *	form, provided that you do not alter this copyright mention in any
 *	way.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <minix/minlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

extern int optind, opterr;
extern char *optarg;

#define USR_MODES (S_ISUID|S_IRWXU)
#define GRP_MODES (S_ISGID|S_IRWXG)
#define EXE_MODES (S_IXUSR|S_IXGRP|S_IXOTH)
#ifdef S_ISVTX
#define ALL_MODES (USR_MODES|GRP_MODES|S_IRWXO|S_ISVTX)
#else
#define ALL_MODES (USR_MODES|GRP_MODES|S_IRWXO)
#endif
#define DEFAULT_MODE (S_IRWXU|S_IRWXG|S_IRWXO)
#define USER_WX (S_IWUSR|S_IXUSR)


/* Global variables */
int pflag;
char *symbolic;
mode_t u_mask;
struct stat st;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(mode_t parsemode, (char *symbolic, Mode_t oldmode));
_PROTOTYPE(int makepath, (char *fordir));
_PROTOTYPE(int makedir, (char *dirname));
_PROTOTYPE(void usage, (void));

/* Parse a P1003.2 4.7.7-conformant symbolic mode. */
mode_t parsemode(symbolic, oldmode)
char *symbolic;
mode_t oldmode;
{
  mode_t who, mask, newmode, tmpmask;
  char action;
  char *end;
  unsigned long octalmode;

  octalmode = strtoul(symbolic, &end, 010);
  if (octalmode < ALL_MODES && *end == 0 && end != symbolic) return octalmode;

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


/* Main module. */
int main(argc, argv)
int argc;
char **argv;
{
  int error, c;

  opterr = 0;
  pflag = 0;
  symbolic = (char *) 0;
  u_mask = umask(0);
  umask(u_mask);
  while ((c = getopt(argc, argv, "m:p")) != EOF) switch (c) {
	    case 'm':	symbolic = optarg;	break;
	    case 'p':	pflag = 1;	break;
	    default:	usage();
	}
  if (optind >= argc) usage();

  error = 0;
  while (optind < argc) error |= makedir(argv[optind++]);
  return(error);
}


/* P1003.2 requires that missing intermediate pathname components should be
 *	created if the -p option is specified (4.40.3).
 */
int makepath(fordir)
char *fordir;
{
  char parent[PATH_MAX + 1], *end;

  strcpy(parent, fordir);
  if (!(end = strrchr(parent, '/'))) return(0);
  *end = '\0';
  if (!parent[0]) return(0);

  if (!stat(parent, &st)) {
	if (S_ISDIR(st.st_mode)) return(0);
	errno = ENOTDIR;
	perror(parent);
	return(1);
  }
  if (mkdir(parent, DEFAULT_MODE)) {
	if (makepath(parent)) return(1);
	if (mkdir(parent, DEFAULT_MODE)) {
		perror(parent);
		return(1);
	}
  }

/* P1003.2 states that, regardless of umask() value, intermediate paths
 *	should have at least write and search (x) permissions (4.40.10).
 */
  if ((u_mask & USER_WX) &&
      chmod(parent, ((~u_mask) | USER_WX)) & DEFAULT_MODE) {
	perror(parent);
	return(1);
  }
  return(0);
}


/* Actual directory creation, using a mkdir() system call. */
int makedir(dirname)
char *dirname;
{
  if (mkdir(dirname, DEFAULT_MODE)) {
	if (!pflag) {
		perror(dirname);
		return(1);
	}
	if (!stat(dirname, &st)) {
		if (S_ISDIR(st.st_mode)) return(0);
		errno = ENOTDIR;
		perror(dirname);
		return(1);
	}
	if (makepath(dirname)) return(1);
	if (mkdir(dirname, DEFAULT_MODE)) {
		perror(dirname);
		return(1);
	}
  }
  if (symbolic && (stat(dirname, &st) ||
		 chmod(dirname, parsemode(symbolic, st.st_mode)))) {
	perror(dirname);
	return(1);
  }
  return(0);
}


/* Posix command prototype. */
void usage()
{
  std_err("Usage: mkdir [-p] [-m mode] dir...\n");
  exit(1);
}
