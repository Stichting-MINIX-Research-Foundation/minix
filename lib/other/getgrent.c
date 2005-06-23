/*	getgrent(), getgrgid(), getgrnam() - group file routines
 *
 *							Author: Kees J. Bot
 *								31 Jan 1994
 */
#define nil 0
#define open _open
#define fcntl _fcntl
#define read _read
#define close _close
#include <sys/types.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

static char GROUP[]= "/etc/group";	/* The group file. */
static const char *grfile;		/* Current group file. */

static char buf[1024];			/* Read buffer. */
static char grline[512];		/* One line from the group file. */
static struct group entry;		/* Entry to fill and return. */
static char *members[64];		/* Group members with the entry. */
static int grfd= -1;			/* Filedescriptor to the file. */
static char *bufptr;			/* Place in buf. */
static ssize_t buflen= 0;		/* Remaining characters in buf. */
static char *lineptr;			/* Place in the line. */

void endgrent(void)
/* Close the group file. */
{
	if (grfd >= 0) {
		(void) close(grfd);
		grfd= -1;
		buflen= 0;
	}
}

int setgrent(void)
/* Open the group file. */
{
	if (grfd >= 0) endgrent();

	if (grfile == nil) grfile= GROUP;

	if ((grfd= open(grfile, O_RDONLY)) < 0) return -1;
	(void) fcntl(grfd, F_SETFD, fcntl(grfd, F_GETFD) | FD_CLOEXEC);
	return 0;
}

void setgrfile(const char *file)
/* Prepare for reading an alternate group file. */
{
	endgrent();
	grfile= file;
}

static int getline(void)
/* Get one line from the group file, return 0 if bad or EOF. */
{
	lineptr= grline;

	do {
		if (buflen == 0) {
			if ((buflen= read(grfd, buf, sizeof(buf))) <= 0)
				return 0;
			bufptr= buf;
		}

		if (lineptr == arraylimit(grline)) return 0;
		buflen--;
	} while ((*lineptr++ = *bufptr++) != '\n');

	lineptr= grline;
	return 1;
}

static char *scan_punct(int punct)
/* Scan for a field separator in a line, return the start of the field. */
{
	char *field= lineptr;
	char *last;

	for (;;) {
		last= lineptr;
		if (*lineptr == 0) return nil;
		if (*lineptr == '\n') break;
		if (*lineptr++ == punct) break;
		if (lineptr[-1] == ':') return nil;	/* :::,,,:,,,? */
	}
	*last= 0;
	return field;
}

struct group *getgrent(void)
/* Read one entry from the group file. */
{
	char *p;
	char **mem;

	/* Open the file if not yet open. */
	if (grfd < 0 && setgrent() < 0) return nil;

	/* Until a good line is read. */
	for (;;) {
		if (!getline()) return nil;	/* EOF or corrupt. */

		if ((entry.gr_name= scan_punct(':')) == nil) continue;
		if ((entry.gr_passwd= scan_punct(':')) == nil) continue;
		if ((p= scan_punct(':')) == nil) continue;
		entry.gr_gid= strtol(p, nil, 0);

		entry.gr_mem= mem= members;
		if (*lineptr != '\n') {
			do {
				if ((*mem= scan_punct(',')) == nil) goto again;
				if (mem < arraylimit(members) - 1) mem++;
			} while (*lineptr != 0);
		}
		*mem= nil;
		return &entry;
	again:;
	}
}

struct group *getgrgid(_mnx_Gid_t gid)
/* Return the group file entry belonging to the user-id. */
{
	struct group *gr;

	endgrent();
	while ((gr= getgrent()) != nil && gr->gr_gid != gid) {}
	endgrent();
	return gr;
}

struct group *getgrnam(const char *name)
/* Return the group file entry belonging to the user name. */
{
	struct group *gr;

	endgrent();
	while ((gr= getgrent()) != nil && strcmp(gr->gr_name, name) != 0) {}
	endgrent();
	return gr;
}
