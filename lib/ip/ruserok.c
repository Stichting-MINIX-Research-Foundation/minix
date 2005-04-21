/*	ruserok() - hosts.equiv and .rhosts check	Author: Kees J. Bot
 *								25 May 2001
 *
 * Under Minix one can use IP addresses, CIDR network blocks, and hostnames
 * with wildcards in .rhosts files.  Only the iruserok() interface can be
 * used, and the IP address is reverse/forward crosschecked if a hostname
 * match is done.  Ruserok() is dead and buried.  The superuser parameter is
 * ignored, because it makes root too special.  Most users on Minix can be
 * root, so hosts.equiv would become useless if root can't use it.  Likewise
 * .rhosts isn't checked to be root or user owned and stuff, users have to
 * be careful themselves.
 */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <pwd.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/netdb.h>
#include <net/gen/inet.h>
#include <net/gen/socket.h>
#include <net/gen/nameser.h>

/* Odd global variable.  Seems to be used by lpd(8). */
int	__check_rhosts_file = 1;

static int cidr_aton(char *word, ipaddr_t *addr, ipaddr_t *mask)
/* Try to interpret 'word' as an CIDR spec, e.g. 172.16.102.64/27. */
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
	lc(cw, *word);
	lc(cp, *pattern);

	if (cp == '*') {
	    do pattern++; while (*pattern == '*');
	    lc(cp, *pattern);
	    if (cp == 0) return 1;

	    while (cw != 0) {
		if (cw == cp && match(word+1, pattern+1)) return 1;
		word++;
		lc(cw, *word);
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
    int ok, i;

    ok= 0;
    he= gethostbyaddr((char *) &addr, sizeof(addr), AF_INET);
    if (he != NULL) {
	strcpy(name, he->h_name);
	he= gethostbyname(name);

	if (he != NULL && he->h_addrtype == AF_INET) {
	    for (i= 0; he->h_addr_list[i] != NULL; i++) {
		if (memcmp(he->h_addr_list[i], &addr, sizeof(addr)) == 0) {
		    ok= 1;
		    break;
		}
	    }
	}
    }
    strcpy(name, ok ? he->h_name : inet_ntoa(addr));
    return ok;
}

int __ivaliduser(FILE *hostf, unsigned long raddr,
    const char *luser, const char *ruser)
{
    register char *p;
    char buf[MAXDNAME + 128];		/* host + login */
    char rhost[MAXDNAME];		/* remote host */
    char *word[2];
    int i, ch, got_name;
    ipaddr_t addr, mask;

    got_name = -1;

    while (fgets(buf, sizeof(buf), hostf)) {
	/* Skip lines that are too long. */
	if (strchr(buf, '\n') == NULL) {
	    while ((ch = fgetc(hostf)) != '\n' && ch != EOF);
	    continue;
	}
	i = 0;
	p = buf;
	for (;;) {
	    while (isspace(*p)) *p++ = '\0';
	    if (*p == '\0') break;
	    if (i < 2) word[i] = p;
	    i++;
	    while (*p != '\0' && !isspace(*p)) p++;
	}
	if (i != 1 && i != 2) continue;
	if (word[0][0] == '#') continue;
	if (strcmp(ruser, i == 2 ? word[1] : luser) != 0) continue;

	if (cidr_aton(word[0], &addr, &mask)) {
	    if (((raddr ^ addr) & mask) == 0) return (0);
	    continue;
	}

	if (got_name == -1) got_name = get_name(raddr, rhost);
	if (match(rhost, word[0])) return (0);
    }
    return (-1);
}

int iruserok(unsigned long raddr, int superuser,
    const char *ruser, const char *luser)
{
    /* Returns 0 if ok, -1 if not ok. */
    struct passwd *pwd;
    FILE *hostf;
    int i, r;
    char pbuf[PATH_MAX];

    for (i = 0; i < 2; i++) {
	if (i == 0) {
	    strcpy(pbuf, _PATH_HEQUIV);
	} else {
	    if (!__check_rhosts_file) return (-1);
	    if ((pwd = getpwnam(luser)) == NULL) return (-1);
	    (void)strcpy(pbuf, pwd->pw_dir);
	    (void)strcat(pbuf, "/.rhosts");
	}

	if ((hostf = fopen(pbuf, "r")) == NULL) return (-1);

	r = __ivaliduser(hostf, raddr, luser, ruser);
	(void)fclose(hostf);
	if (r == 0) return (0);
    }
    return (-1);
}
