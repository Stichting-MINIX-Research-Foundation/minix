/*
ttyslot.c

Return the index in the utmp file for the current user's terminal. The 
current user's terminal is the first file descriptor in the range 0..2
for which ttyname() returns a name. The index is the line number in the
/etc/ttytab file. 0 will be returned in case of an error.

Created:	Oct 11, 1992 by Philip Homburg
*/

#define _MINIX_SOURCE

#include <sys/types.h>
#include <ttyent.h>
#include <string.h>
#include <unistd.h>

int ttyslot()
{
	int slot;

	slot= fttyslot(0);
	if (slot == 0) slot= fttyslot(1);
	if (slot == 0) slot= fttyslot(2);
	return slot;
}

int fttyslot(fd)
int fd;
{
	char *tname;
	int lineno;
	struct ttyent *ttyp;

	tname= ttyname(fd);
	if (tname == NULL) return 0;

	/* Assume that tty devices are in /dev */
	if (strncmp(tname, "/dev/", 5) != 0)
		return 0;	/* Malformed tty name. */
	tname += 5;

	/* Scan /etc/ttytab. */
	lineno= 1;
	while ((ttyp= getttyent()) != NULL)
	{
		if (strcmp(tname, ttyp->ty_name) == 0)
		{
			endttyent();
			return lineno;
		}
		lineno++;
	}
	/* No match */
	endttyent();
	return 0;
}

/*
 * $PchHeader: /mount/hd2/minix/lib/misc/RCS/ttyslot.c,v 1.3 1994/12/22 13:49:12 philip Exp $
 */
