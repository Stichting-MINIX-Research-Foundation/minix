/*	getttyent(3) - get a ttytab entry		Author: Kees J. Bot
 *								28 Oct 1995
 */
#define nil 0
#define open _open
#define close _close
#define fcntl _fcntl
#define read _read
#include <string.h>
#include <sys/types.h>
#include <ttyent.h>
#include <unistd.h>
#include <fcntl.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

static char TTYTAB[]= "/etc/ttytab";	/* The table of terminal devices. */

static char buf[512];			/* Read buffer. */
static char ttline[256];		/* One line from the ttytab file. */
static char *ttargv[32];		/* Compound arguments. */
static struct ttyent entry;		/* Entry to fill and return. */
static int ttfd= -1;			/* Filedescriptor to the file. */
static char *bufptr;			/* Place in buf. */
static ssize_t buflen= 0;		/* Remaining characters in buf. */
static char *lineptr;			/* Place in the line. */
static char **argvptr;			/* Place in word lists. */

void endttyent(void)
/* Close the ttytab file. */
{
	if (ttfd >= 0) {
		(void) close(ttfd);
		ttfd= -1;
		buflen= 0;
	}
}

int setttyent(void)
/* Open the ttytab file. */
{
	if (ttfd >= 0) endttyent();

	if ((ttfd= open(TTYTAB, O_RDONLY)) < 0) return -1;
	(void) fcntl(ttfd, F_SETFD, fcntl(ttfd, F_GETFD) | FD_CLOEXEC);
	return 0;
}

static int getline(void)
/* Get one line from the ttytab file, return 0 if bad or EOF. */
{
	lineptr= ttline;
	argvptr= ttargv;

	do {
		if (buflen == 0) {
			if ((buflen= read(ttfd, buf, sizeof(buf))) <= 0)
				return 0;
			bufptr= buf;
		}

		if (lineptr == arraylimit(ttline)) return 0;
		buflen--;
	} while ((*lineptr++ = *bufptr++) != '\n');

	lineptr= ttline;
	return 1;
}

static int white(int c)
/* Whitespace? */
{
	return c == ' ' || c == '\t';
}

static char *scan_white(int quoted)
/* Scan for a field separator in a line, return the start of the field.
 * "quoted" is set if we have to watch out for double quotes.
 */
{
	char *field, *last;

	while (white(*lineptr)) lineptr++;
	if (!quoted && *lineptr == '#') return nil;

	field= lineptr;
	for (;;) {
		last= lineptr;
		if (*lineptr == 0) return nil;
		if (*lineptr == '\n') break;
		if (quoted && *lineptr == '"') return field;
		if (white(*lineptr++)) break;
	}
	*last= 0;
	return *field == 0 ? nil : field;
}

static char **scan_quoted(void)
/* Read a field that may be a quoted list of words. */
{
	char *p, **field= argvptr;

	while (white(*lineptr)) lineptr++;

	if (*lineptr == '"') {
		/* Quoted list of words. */
		lineptr++;
		while ((p= scan_white(1)) != nil && *p != '"') {
			if (argvptr == arraylimit(ttargv)) return nil;
			*argvptr++= p;
		}
		if (*lineptr == '"') *lineptr++= 0;
	} else {
		/* Just one word. */
		if ((p= scan_white(0)) == nil) return nil;
		if (argvptr == arraylimit(ttargv)) return nil;
		*argvptr++= p;
	}
	if (argvptr == arraylimit(ttargv)) return nil;
	*argvptr++= nil;
	return field;
}

struct ttyent *getttyent(void)
/* Read one entry from the ttytab file. */
{
	/* Open the file if not yet open. */
	if (ttfd < 0 && setttyent() < 0) return nil;

	/* Look for a line with something on it. */
	for (;;) {
		if (!getline()) return nil;	/* EOF or corrupt. */

		if ((entry.ty_name= scan_white(0)) == nil) continue;
		entry.ty_type= scan_white(0);
		entry.ty_getty= scan_quoted();
		entry.ty_init= scan_quoted();

		return &entry;
	}
}

struct ttyent *getttynam(const char *name)
/* Return the ttytab file entry for a given tty. */
{
	struct ttyent *tty;

	endttyent();
	while ((tty= getttyent()) != nil && strcmp(tty->ty_name, name) != 0) {}
	endttyent();
	return tty;
}
