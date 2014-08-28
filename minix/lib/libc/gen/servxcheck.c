/*	servxcheck() - Service access check.		Author: Kees J. Bot
 *								8 Jan 1997
 */
#define nil 0
#define ioctl _ioctl
#define open _open
#define write _write
#define close _close
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <minix/paths.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/inet.h>
#include <net/gen/socket.h>
#include <netdb.h>

/* Default service access file. */
static const char *path_servacces = _PATH_SERVACCES;

#define WLEN	256

static int getword(FILE *fp, char *word)
/* Read a word from the file open by 'fp', skip whitespace and comments.
 * Colon and semicolon are returned as a one character "word".  Returns
 * word[0] or EOF.
 */
{
    int c;
    char *pw;
    int wc;

    wc= 0;
    for (;;) {
	if ((c= getc(fp)) == EOF) return EOF;
	if (c == '#') { wc= 1; continue; }
	if (c == '\n') { wc= 0; continue; }
	if (wc) continue;
	if (c <= ' ') continue;
	break;
    }

    pw= word;
    if (c == ':' || c == ';') {
	    *pw++ = c;
    } else {
	do {
	    if (pw < word + WLEN-1) *pw++ = c;
	    c= getc(fp);
	} while (c != EOF && c > ' ' && c != ':' && c != ';');
	if (c != EOF) ungetc(c, fp);
    }
    *pw= 0;
    return word[0];
}

static int netspec(char *word, ipaddr_t *addr, ipaddr_t *mask)
/* Try to interpret 'word' as an network spec, e.g. 172.16.102.64/27. */
{
    char *slash;
    int r;
    static char S32[]= "/32";

    if (*word == 0) return 0;

    if ((slash= strchr(word, '/')) == NULL) slash= S32;

    *slash= 0;
    r= inet_aton(word, addr);
    *slash++= '/';
    if (!r) return 0;

    r= 0;
    while ((*slash - '0') < 10u) {
	r= 10*r + (*slash++ - '0');
	if (r > 32) return 0;
    }
    if (*slash != 0 || slash[-1] == '/') return 0;
    *mask= htonl(r == 0 ? 0L : (0xFFFFFFFFUL >> (32 - r)) << (32 - r));
    return 1;
}

static int match(const char *word, const char *pattern)
/* Match word onto a pattern.  Pattern may contain the * wildcard. */
{
    unsigned cw, cp;
#define lc(c, d) ((((c)= (d)) - 'A') <= ('Z' - 'A') ? (c)+= ('a' - 'A') : 0)

    for (;;) {
	(void) lc(cw, *word);
	(void) lc(cp, *pattern);

	if (cp == '*') {
	    do pattern++; while (*pattern == '*');
	    (void) lc(cp, *pattern);
	    if (cp == 0) return 1;

	    while (cw != 0) {
		if (cw == cp && match(word+1, pattern+1)) return 1;
		word++;
		(void) lc(cw, *word);
	    }
	    return 0;
	} else
	if (cw == 0 || cp == 0) {
	    return cw == cp;
	} else
	if (cw == cp) {
	    word++;
	    pattern++;
	} else {
	    return 0;
	}
    }
#undef lc
}

static int get_name(ipaddr_t addr, char *name)
/* Do a reverse lookup on the remote IP address followed by a forward lookup
 * to check if the host has that address.  Return true if this is so, return
 * either the true name or the ascii IP address in name[].
 */
{
    struct hostent *he;
    int i;

    he= gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
    if (he != NULL) {
	strcpy(name, he->h_name);
	he= gethostbyname(name);

	if (he != NULL && he->h_addrtype == AF_INET) {
	    for (i= 0; he->h_addr_list[i] != NULL; i++) {
		if (memcmp(he->h_addr_list[i], &addr, sizeof(addr)) == 0) {
		    strcpy(name, he->h_name);
		    return 1;
		}
	    }
	}
    }
    strcpy(name, inet_ntoa(addr));
    return 0;
}

/* "state" and "log" flags, made to be bitwise comparable. */
#define DEFFAIL		 0x01
#define FAIL		(0x02 | DEFFAIL)
#define PASS		 0x04

int servxcheck(unsigned long peer, const char *service,
		void (*logf)(int pass, const char *name))
{
    FILE *fp;
    char word[WLEN];
    char name[WLEN];
    int c;
    int got_name, slist, seen, explicit, state, log;
    ipaddr_t addr, mask;

    /* Localhost? */
    if ((peer & htonl(0xFF000000)) == htonl(0x7F000000)) return 1;

    if ((fp= fopen(path_servacces, "r")) == nil) {
	/* Succeed on error, fail if simply nonexistent. */
	return (errno != ENOENT);
    }

    slist= 1;		/* Services list (before the colon.) */
    seen= 0;		/* Given service not yet seen. */
    explicit= 0;	/* Service mentioned explicitly. */
    got_name= -1;	/* No reverse lookup done yet. */
    log= FAIL;		/* By default log failures only. */
    state= DEFFAIL;	/* Access denied until we know better. */

    while ((c= getword(fp, word)) != EOF) {
	if (c == ':') {
	    slist= 0;		/* Switch to access list. */
	} else
	if (c == ';') {
	    slist= 1;		/* Back to list of services. */
	    seen= 0;
	} else
	if (slist) {
	    /* Traverse services list. */

	    if (match(service, word)) {
		/* Service has been spotted! */
		if (match(word, service)) {
		    /* Service mentioned without wildcards. */
		    seen= explicit= 1;
		} else {
		    /* Matched by a wildcard. */
		    if (!explicit) seen= 1;
		}
	    }
	} else {
	    /* Traverse access list. */

	    if (c == 'l' && strcmp(word, "log") == 0) {
		if (seen) {
		    /* Log failures and successes. */
		    log= FAIL|PASS;
		}
		continue;
	    }

	    if (c != '-' && c != '+') {
		if (logf == nil) {
		    syslog(LOG_ERR, "%s: strange check word '%s'\n",
			path_servacces, word);
		}
		continue;
	    }

	    if (seen) {
		if (state == DEFFAIL) {
		    /* First check determines the default. */
		    state= c == '+' ? FAIL : PASS;
		}

		if ((state == PASS) == (c == '+')) {
		    /* This check won't change state. */
		} else
		if (word[1] == 0) {
		    /* Lone + or - allows all or none. */
		    state= c == '-' ? FAIL : PASS;
		} else
		if (netspec(word+1, &addr, &mask)) {
		    /* Remote host is on the specified network? */
		    if (((peer ^ addr) & mask) == 0) {
			state= c == '-' ? FAIL : PASS;
		    }
		} else {
		    /* Name check. */
		    if (got_name == -1) {
			got_name= get_name(peer, name);
		    }

		    /* Remote host name matches the word? */
		    if (!got_name) {
			state= FAIL;
		    } else
		    if (match(name, word+1)) {
			state= c == '-' ? FAIL : PASS;
		    }
		}
	    }
	}
    }
    fclose(fp);

    if ((log & state) != 0) {
	/* Log the result of the check. */
	if (got_name == -1) (void) get_name(peer, name);

	if (logf != nil) {
	    (*logf)(state == PASS, name);
	} else {
	    syslog(LOG_NOTICE, "service '%s' %s to %s\n",
		service, state == PASS ? "granted" : "denied", name);
	}
    }
    return state == PASS;
}

char *servxfile(const char *file)
/* Specify a file to use for the access checks other than the default.  Return
 * the old path.
 */
{
    const char *oldpath= path_servacces;
    path_servacces= file;
    return (char *) oldpath;	/* (avoid const poisoning) */
}
