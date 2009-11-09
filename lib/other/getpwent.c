/*	getpwent(), getpwuid(), getpwnam() - password file routines
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
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

static char PASSWD[]= "/etc/passwd";	/* The password file. */
static const char *pwfile;		/* Current password file. */

static char buf[1024];			/* Read buffer. */
static char pwline[256];		/* One line from the password file. */
static struct passwd entry;		/* Entry to fill and return. */
static int pwfd= -1;			/* Filedescriptor to the file. */
static char *bufptr;			/* Place in buf. */
static ssize_t buflen= 0;		/* Remaining characters in buf. */
static char *lineptr;			/* Place in the line. */

void endpwent(void)
/* Close the password file. */
{
	if (pwfd >= 0) {
		(void) close(pwfd);
		pwfd= -1;
		buflen= 0;
	}
}

int setpwent(void)
/* Open the password file. */
{
	if (pwfd >= 0) endpwent();

	if (pwfile == nil) pwfile= PASSWD;

	if ((pwfd= open(pwfile, O_RDONLY)) < 0) return -1;
	(void) fcntl(pwfd, F_SETFD, fcntl(pwfd, F_GETFD) | FD_CLOEXEC);
	return 0;
}

void setpwfile(const char *file)
/* Prepare for reading an alternate password file. */
{
	endpwent();
	pwfile= file;
}

static int getline(void)
/* Get one line from the password file, return 0 if bad or EOF. */
{
	lineptr= pwline;

	do {
		if (buflen == 0) {
			if ((buflen= read(pwfd, buf, sizeof(buf))) <= 0)
				return 0;
			bufptr= buf;
		}

		if (lineptr == arraylimit(pwline)) return 0;
		buflen--;
	} while ((*lineptr++ = *bufptr++) != '\n');

	lineptr= pwline;
	return 1;
}

static char *scan_colon(void)
/* Scan for a field separator in a line, return the start of the field. */
{
	char *field= lineptr;
	char *last;

	for (;;) {
		last= lineptr;
		if (*lineptr == 0) return nil;
		if (*lineptr == '\n') break;
		if (*lineptr++ == ':') break;
	}
	*last= 0;
	return field;
}

struct passwd *getpwent(void)
/* Read one entry from the password file. */
{
	char *p;

	/* Open the file if not yet open. */
	if (pwfd < 0 && setpwent() < 0) return nil;

	/* Until a good line is read. */
	for (;;) {
		if (!getline()) return nil;	/* EOF or corrupt. */

		if ((entry.pw_name= scan_colon()) == nil) continue;
		if ((entry.pw_passwd= scan_colon()) == nil) continue;
		if ((p= scan_colon()) == nil) continue;
		entry.pw_uid= strtol(p, nil, 0);
		if ((p= scan_colon()) == nil) continue;
		entry.pw_gid= strtol(p, nil, 0);
		if ((entry.pw_gecos= scan_colon()) == nil) continue;
		if ((entry.pw_dir= scan_colon()) == nil) continue;
		if ((entry.pw_shell= scan_colon()) == nil) continue;

		if (*lineptr == 0) return &entry;
	}
}

struct passwd *getpwuid(_mnx_Uid_t uid)
/* Return the password file entry belonging to the user-id. */
{
	struct passwd *pw;

	endpwent();
	while ((pw= getpwent()) != nil && pw->pw_uid != uid) {}
	endpwent();
	return pw;
}

struct passwd *getpwnam(const char *name)
/* Return the password file entry belonging to the user name. */
{
	struct passwd *pw;

	endpwent();
	while ((pw= getpwent()) != nil && strcmp(pw->pw_name, name) != 0) {}
	endpwent();
	return pw;
}
