/*
 * openpty() tries to open a pty; applications won't have to
 * duplicate this code all the time (or change it if the system
 * pty interface changes).
 *
 * First version by Ben Gras <beng@few.vu.nl>,
 * Initially heavily based on telnetd/pty.c
 * by Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>.
 *
 */
#include <libutil.h>
#include <termios.h>
#include <fcntl.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioc_tty.h>

#define DEV_DIR		"/dev"

/*
 * Allocate a PTY, by trying to open one repeatedly.
 */
int openpty(int *amaster, int *aslave, char *name,
	struct termios *termp, struct winsize *winp)
{
  char buff[128], temp[128];
  register int i, j;
  int pty_fd = -1, gr;
  static char tty_name[128];
  struct group *ttygroup;
  gid_t tty_gid = 0;

  if(!amaster || !aslave) {
  	errno = EINVAL;
  	return -1;
  }

  for(i = 'p'; i < 'w'; i++) {
	j = 0;
	do {
		sprintf(buff, "%s/pty%c%c",
			DEV_DIR, i, (j < 10) ? j + '0' : j + 'a' - 10);

		if((*amaster = open(buff, O_RDWR)) >= 0) {
		  sprintf(tty_name, "%s/tty%c%c", DEV_DIR,
			i, (j < 10) ? j + '0' : j + 'a' - 10);
		  if((*aslave = open(tty_name, O_RDWR)) >= 0) {
		  	break;
		  }
		  close(*amaster);
		}

		j++;
		if (j == 16) break;
	} while(1);

	/* Did we find one? */
	if (j < 16) break;
  }
  if (*amaster < 0) { errno = ENOENT; return(-1); }

  setgrent();
  ttygroup = getgrnam("tty");
  endgrent();
  if(ttygroup) tty_gid = ttygroup->gr_gid;

  if(name) strcpy(name, tty_name);

  /* Ignore errors on these. */
  chown(tty_name, getuid(), tty_gid);
  chmod(tty_name, 0620);	/* -rw--w---- */
  if(termp) tcsetattr(*aslave, TCSAFLUSH, termp);
  if(winp) ioctl(*aslave, TIOCSWINSZ, winp);

  return(0);
}

