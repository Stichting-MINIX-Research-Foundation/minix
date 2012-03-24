/* mesg - enable or disable communications. Author: John J. Ribera */

/*
 * mesg -	enable or disable communications.
 *
 * Usage:	mesg [ y | n ]
 *
 *		'mesg n' will turn off group and world permissions of the
 *			 user's terminal.
 *		'mesg y' will enable group and world to write to the user's
 *			 tty.
 *		mesg	 with no parameters will put the writeable status
 *			 onto stdout.
 *
 * Author:	John J. Ribera, Jr. 09/09/90
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv []);

int main(argc, argv)
int  argc;
char *argv[];
{
  struct stat statb;
  char *tty_name;

  if ((tty_name = ttyname(0)) == NULL) exit(2);
  if (stat(tty_name, &statb) == -1) exit(2);
  if (--argc) {
	if (*argv[1] == 'n') statb.st_mode = 0600;
	  else if (*argv[1] == 'y') statb.st_mode = 0620;
	  else {
		fprintf(stderr, "mesg: usage: mesg [n|y]\n");
		exit(2);
	}
	if (chmod(tty_name, statb.st_mode) == -1) exit(2);
  } else printf((statb.st_mode & 020) ? "is y\n" : "is n\n");

  if (statb.st_mode & 020) exit(0);

  exit(1);
}

