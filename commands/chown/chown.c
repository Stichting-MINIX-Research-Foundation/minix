/* chown/chgrp - Change file ownership			Author: V. Archer */

/* Copyright 1991 by Vincent Archer
 *	You may freely redistribute this software, in source or binary
 *	form, provided that you do not alter this copyright mention in any
 *	way.
 */

/* Changed  3 Feb 93 by Kees J. Bot:  setuid execution nonsense removed.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <minix/minlib.h>
#include <stdio.h>

#ifndef S_ISLNK
#define S_ISLNK(mode)	0
#define lstat		stat
#endif

#define S_IUGID (S_ISUID|S_ISGID)

/* Global variables, such as flags and path names */
int gflag, oflag, rflag, error;
char *pgmname, path[PATH_MAX + 1];
uid_t nuid;
gid_t ngid;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void do_chown, (char *file));
_PROTOTYPE(void usage, (void));

/* Main module. If chown(1) is invoked as chgrp(1), the behaviour is nearly
 * identical, except that the default when a single name is given as an
 * argument is to take a group id rather than an user id. This allow the
 * non-Posix "chgrp user:group file".
 * The single option switch used by chown/chgrp (-R) does not warrant a
 * call to the getopt stuff. The two others flags (-g, -u) are set from
 * the program name and arguments.
 */
int main(argc, argv)
int argc;
char *argv[];
{
  char *id, *id2;
  struct group *grp;
  struct passwd *pwp;

  if (pgmname = strrchr(*argv, '/'))
	pgmname++;
  else
	pgmname = *argv;
  argc--;
  argv++;
  gflag = strcmp(pgmname, "chgrp");

  if (argc && **argv == '-' && argv[0][1] == 'R') {
	argc--;
	argv++;
	rflag = 1;
  }
  if (argc < 2) usage();

  id = *argv++;
  argc--;
  if (id2 = strchr(id, ':')) *id2++ = '\0';
  if (!id2 && !gflag) {
	id2 = id;
	id = 0;
  }
  if (id) {
	if (isdigit(*id))
		nuid = atoi(id);
	else {
		if (!(pwp = getpwnam(id))) {
			std_err(id);
			std_err(": unknown user name\n");
			exit(1);
		}
		nuid = pwp->pw_uid;
	}
	oflag = 1;
  } else
	oflag = 0;

  if (id2) {
	if (isdigit(*id2))
		ngid = atoi(id2);
	else {
		if (!(grp = getgrnam(id2))) {
			std_err(id2);
			std_err(": unknown group name\n");
			exit(1);
		}
		ngid = grp->gr_gid;
	}
	gflag = 1;
  } else
	gflag = 0;

  error = 0;
  while (argc--) do_chown(*argv++);
  return(error);
}

/* Apply the user/group modification here.
 */
void do_chown(file)
char *file;
{
  DIR *dirp;
  struct dirent *entp;
  char *namp;
  struct stat st;

  if (lstat(file, &st)) {
	perror(file);
	error = 1;
	return;
  }

  if (S_ISLNK(st.st_mode) && rflag) return;	/* Note: violates POSIX. */

  if (chown(file, oflag ? nuid : st.st_uid, gflag ? ngid : st.st_gid)) {
	perror(file);
	error = 1;
  }

  if (S_ISDIR(st.st_mode) && rflag) {
	if (!(dirp = opendir(file))) {
		perror(file);
		error = 1;
		return;
	}
	if (path != file) strcpy(path, file);
	namp = path + strlen(path);
	*namp++ = '/';
	while (entp = readdir(dirp))
		if (entp->d_name[0] != '.' ||
		    (entp->d_name[1] &&
		     (entp->d_name[1] != '.' || entp->d_name[2]))) {
			strcpy(namp, entp->d_name);
			do_chown(path);
		}
	closedir(dirp);
	*--namp = '\0';
  }
}

/* Posix prototype of the chown/chgrp function */
void usage()
{
  std_err("Usage: ");
  std_err(pgmname);
  std_err(gflag ? " owner[:group]" : " [owner:]group");
  std_err(" file...\n");
  exit(1);
}
