/*	man 2.4 - display online manual pages		Author: Kees J. Bot
 *								17 Mar 1993
 */
#define nil NULL
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Defaults: */
char MANPATH[]=	"/usr/local/man:/usr/man";
char PAGER[]=	"more";

/* Comment at the start to let tbl(1) be run before n/troff. */
char TBL_MAGIC[] = ".\\\"t\n";

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))
#define between(a, c, z) ((unsigned) ((c) - (a)) <= (unsigned) ((z) - (a)))

/* Section 9 uses special macros under Minix. */
#if __minix
#define SEC9SPECIAL	1
#else
#define SEC9SPECIAL	0
#endif

int searchwhatis(FILE *wf, char *title, char **ppage, char **psection)
/* Search a whatis file for the next occurence of "title".  Return the basename
 * of the page to read and the section it is in.  Return 0 on failure, 1 on
 * success, -1 on EOF or error.
 */
{
    static char page[256], section[32];
    char alias[256];
    int found= 0;
    int c;

    /* Each whatis line should have the format:
     *	page, title, title (section) - descriptive text
     */

    /* Search the file for a line with the title. */
    do {
	int first= 1;
	char *pc= section;

	c= fgetc(wf);

	/* Search the line for the title. */
	do {
	    char *pa= alias;

	    while (c == ' ' || c == '\t' || c == ',') c= fgetc(wf);

	    while (c != ' ' && c != '\t' && c != ','
		    && c != '(' && c != '\n' && c != EOF
	    ) {
		if (pa < arraylimit(alias)-1) *pa++= c;
		c= getc(wf);
	    }
	    *pa= 0;
	    if (first) { strcpy(page, alias); first= 0; }

	    if (strcmp(alias, title) == 0) found= 1;
	} while (c != '(' && c != '\n' && c != EOF);

	if (c != '(') {
	    found= 0;
	} else {
	    while ((c= fgetc(wf)) != ')' && c != '\n' && c != EOF) {
		if ('A' <= c && c <= 'Z') c= c - 'A' + 'a';
		if (pc < arraylimit(section)-1) *pc++= c;
	    }
	    *pc= 0;
	    if (c != ')' || pc == section) found= 0;
	}
	while (c != EOF && c != '\n') c= getc(wf);
    } while (!found && c != EOF);

    if (found) {
	*ppage= page;
	*psection= section;
    }
    return c == EOF ? -1 : found;
}

int searchwindex(FILE *wf, char *title, char **ppage, char **psection)
/* Search a windex file for the next occurence of "title".  Return the basename
 * of the page to read and the section it is in.  Return 0 on failure, 1 on
 * success, -1 on EOF or error.
 */
{
    static char page[256], section[32];
    static long low, high;
    long mid0, mid1;
    int c;
    unsigned char *pt;
    char *pc;

    /* Each windex line should have the format:
     *	title page (section) - descriptive text
     * The file is sorted.
     */

    if (ftell(wf) == 0) {
	/* First read of this file, initialize. */
	low= 0;
	fseek(wf, (off_t) 0, SEEK_END);
	high= ftell(wf);
    }

    /* Binary search for the title. */
    while (low <= high) {
	pt= (unsigned char *) title;

	mid0= mid1= (low + high) >> 1;
	if (mid0 == 0) {
	    if (fseek(wf, (off_t) 0, SEEK_SET) != 0)
		return -1;
	} else {
	    if (fseek(wf, (off_t) mid0 - 1, SEEK_SET) != 0)
		return -1;

	    /* Find the start of a line. */
	    while ((c= getc(wf)) != EOF && c != '\n')
		mid1++;
	    if (ferror(wf)) return -1;
	}

	/* See if the line has the title we seek. */
	for (;;) {
	    if ((c= getc(wf)) == ' ' || c == '\t') c= 0;
	    if (c == 0 || c != *pt) break;
	    pt++;
	}

	/* Halve the search range. */
	if (c == EOF || *pt <= c) {
	    high= mid0 - 1;
	} else {
	    low= mid1 + 1;
	}
    }

    /* Look for the title from 'low' onwards. */
    if (fseek(wf, (off_t) low, SEEK_SET) != 0)
	return -1;

    do {
	if (low != 0) {
	    /* Find the start of a line. */
	    while ((c= getc(wf)) != EOF && c != '\n')
		low++;
	    if (ferror(wf)) return -1;
	}

	/* See if the line has the title we seek. */
	pt= (unsigned char *) title;

	for (;;) {
	    if ((c= getc(wf)) == EOF) return 0;
	    low++;
	    if (c == ' ' || c == '\t') c= 0;
	    if (c == 0 || c != *pt) break;
	    pt++;
	}
    } while (c < *pt);

    if (*pt != c) return 0;		/* Not found. */

    /* Get page and section. */
    while ((c= fgetc(wf)) == ' ' || c == '\t') {}

    pc= page;
    while (c != ' ' && c != '\t' && c != '(' && c != '\n' && c != EOF) {
	if (pc < arraylimit(page)-1) *pc++= c;
	c= getc(wf);
    }
    if (pc == page) return 0;
    *pc= 0;

    while (c == ' ' || c == '\t') c= fgetc(wf);

    if (c != '(') return 0;

    pc= section;
    while ((c= fgetc(wf)) != ')' && c != '\n' && c != EOF) {
	if ('A' <= c && c <= 'Z') c= c - 'A' + 'a';
	if (pc < arraylimit(section)-1) *pc++= c;
    }
    *pc= 0;
    if (c != ')' || pc == section) return 0;

    while (c != EOF && c != '\n') c= getc(wf);
    if (c != '\n') return 0;

    *ppage= page;
    *psection= section;
    return 1;
}

char ALL[]= "";		/* Magic sequence of all sections. */

int all= 0;		/* Show all pages with a given title. */
int whatis= 0;		/* man -f word == whatis word. */
int apropos= 0;		/* man -k word == apropos word. */
int quiet= 0;		/* man -q == quietly check. */
enum ROFF { NROFF, TROFF } rofftype= NROFF;
char *roff[] = { "nroff", "troff" };

int shown;		/* True if something has been shown. */
int tty;		/* True if displaying on a terminal. */
char *manpath;		/* The manual directory path. */
char *pager;		/* The pager to use. */

char *pipeline[8][8];	/* An 8 command pipeline of 7 arguments each. */
char *(*plast)[8] = pipeline;

void putinline(char *arg1, ...)
/* Add a command to the pipeline. */
{
    va_list ap;
    char **argv;

    argv= *plast++;
    *argv++= arg1;

    va_start(ap, arg1);
    while ((*argv++= va_arg(ap, char *)) != nil) {}
    va_end(ap);
}

void execute(int set_mp, char *file)
/* Execute the pipeline build with putinline().  (This is a lot of work to
 * avoid a call to system(), but it so much fun to do it right!)
 */
{
    char *(*plp)[8], **argv;
    char *mp;
    int fd0, pfd[2], err[2];
    pid_t pid;
    int r, status;
    int last;
    void (*isav)(int sig), (*qsav)(int sig), (*tsav)(int sig);

    if (tty) {
	/* Must run this through a pager. */
	putinline(pager, (char *) nil);
    }
    if (plast == pipeline) {
	/* No commands at all? */
	putinline("cat", (char *) nil);
    }

    /* Add the file as argument to the first command. */
    argv= pipeline[0];
    while (*argv != nil) argv++;
    *argv++= file;
    *argv= nil;

    /* Start the commands. */
    fd0= 0;
    for (plp= pipeline; plp < plast; plp++) {
	argv= *plp;
	last= (plp+1 == plast);

	/* Create an error pipe and pipe between this command and the next. */
	if (pipe(err) < 0 || (!last && pipe(pfd) < 0)) {
	    fprintf(stderr, "man: can't create a pipe: %s\n", strerror(errno));
	    exit(1);
	}

	(void) fcntl(err[1], F_SETFD, fcntl(err[1], F_GETFD) | FD_CLOEXEC);

	if ((pid = fork()) < 0) {
	    fprintf(stderr, "man: cannot fork: %s\n", strerror(errno));
	    exit(1);
	}
	if (pid == 0) {
	    /* Child. */
	    if (set_mp) {
		mp= malloc((8 + strlen(manpath) + 1) * sizeof(*mp));
		if (mp != nil) {
		    strcpy(mp, "MANPATH=");
		    strcat(mp, manpath);
		    (void) putenv(mp);
		}
	    }

	    if (fd0 != 0) {
		dup2(fd0, 0);
		close(fd0);
	    }
	    if (!last) {
		close(pfd[0]);
		if (pfd[1] != 1) {
		    dup2(pfd[1], 1);
		    close(pfd[1]);
		}
	    }
	    close(err[0]);
	    execvp(argv[0], argv);
	    (void) write(err[1], &errno, sizeof(errno));
	    _exit(1);
	}

	close(err[1]);
	if (read(err[0], &errno, sizeof(errno)) != 0) {
	    fprintf(stderr, "man: %s: %s\n", argv[0],
			    strerror(errno));
	    exit(1);
	}
	close(err[0]);

	if (!last) {
	    close(pfd[1]);
	    fd0= pfd[0];
	}
	set_mp= 0;
    }

    /* Wait for the last command to finish. */
    isav= signal(SIGINT, SIG_IGN);
    qsav= signal(SIGQUIT, SIG_IGN);
    tsav= signal(SIGTERM, SIG_IGN);
    while ((r= wait(&status)) != pid) {
	if (r < 0) {
	    fprintf(stderr, "man: wait(): %s\n", strerror(errno));
	    exit(1);
	}
    }
    (void) signal(SIGINT, isav);
    (void) signal(SIGQUIT, qsav);
    (void) signal(SIGTERM, tsav);
    if (status != 0) exit(1);
    plast= pipeline;
}

void keyword(char *keyword)
/* Make an apropos(1) or whatis(1) call. */
{
    putinline(apropos ? "apropos" : "whatis",
		all ? "-a" : (char *) nil,
		(char *) nil);

    if (tty) {
	printf("Looking for keyword '%s'\n", keyword);
	fflush(stdout);
    }

    execute(1, keyword);
}

enum pagetype { CAT, CATZ, MAN, MANZ, SMAN, SMANZ };

int showpage(char *page, enum pagetype ptype, char *macros)
/* Show a manual page if it exists using the proper decompression and
 * formatting tools.
 */
{
    struct stat st;

    /* We want a normal file without X bits if not a full path. */
    if (stat(page, &st) < 0) return 0;

    if (!S_ISREG(st.st_mode)) return 0;
    if ((st.st_mode & 0111) && page[0] != '/') return 0;

    /* Do we only care if it exists? */
    if (quiet) { shown= 1; return 1; }

    if (ptype == CATZ || ptype == MANZ || ptype == SMANZ) {
	putinline("zcat", (char *) nil);
    }

    if (ptype == SMAN || ptype == SMANZ) {
	/* Change SGML into regular *roff. */
	putinline("/usr/lib/sgml/sgml2roff", (char *) nil);
	putinline("tbl", (char *) nil);
	putinline("eqn", (char *) nil);
    }

    if (ptype == MAN) {
	/* Do we need tbl? */
	FILE *fp;
	int c;
	char *tp = TBL_MAGIC;

	if ((fp = fopen(page, "r")) == nil) {
	    fprintf(stderr, "man: %s: %s\n", page, strerror(errno));
	    exit(1);
	}
	c= fgetc(fp);
	for (;;) {
	    if (c == *tp || (c == '\'' && *tp == '.')) {
		if (*++tp == 0) {
		    /* A match, add tbl. */
		    putinline("tbl", (char *) nil);
		    break;
		}
	    } else {
		/* No match. */
		break;
	    }
	    while ((c = fgetc(fp)) == ' ' || c == '\t') {}
	}
	fclose(fp);
    }

    if (ptype == MAN || ptype == MANZ || ptype == SMAN || ptype == SMANZ) {
	putinline(roff[rofftype], macros, (char *) nil);
    }

    if (tty) {
	printf("%s %s\n",
	    ptype == CAT || ptype == CATZ ? "Showing" : "Formatting", page);
	fflush(stdout);
    }
    execute(0, page);

    shown= 1;
    return 1;
}

int member(char *word, char *list)
/* True if word is a member of a comma separated list. */
{
    size_t len= strlen(word);

    if (list == ALL) return 1;

    while (*list != 0) {
	if (strncmp(word, list, len) == 0
		&& (list[len] == 0 || list[len] == ','))
	    return 1;
	while (*list != 0 && *list != ',') list++;
	if (*list == ',') list++;
    }
    return 0;
}

int trymandir(char *mandir, char *title, char *section)
/* Search the whatis file of the manual directory for a page of the given
 * section and display it.
 */
{
    FILE *wf;
    char whatis[1024], pagename[1024], *wpage, *wsection;
    int rsw, rsp;
    int ntries;
    int (*searchidx)(FILE *, char *, char **, char **);
    struct searchnames {
	enum pagetype	ptype;
	char		*pathfmt;
    } *sp;
    static struct searchnames searchN[] = {
	{ CAT,	"%s/cat%s/%s.%s"	},	/* SysV */
	{ CATZ,	"%s/cat%s/%s.%s.Z"	},
	{ MAN,	"%s/man%s/%s.%s"	},
	{ MANZ,	"%s/man%s/%s.%s.Z"	},
	{ SMAN,	"%s/sman%s/%s.%s"	},	/* Solaris */
	{ SMANZ,"%s/sman%s/%s.%s.Z"	},
	{ CAT,	"%s/cat%.1s/%s.%s"	},	/* BSD */
	{ CATZ,	"%s/cat%.1s/%s.%s.Z"	},
	{ MAN,	"%s/man%.1s/%s.%s"	},
	{ MANZ,	"%s/man%.1s/%s.%s.Z"	},
    };

    if (strlen(mandir) + 1 + 6 + 1 > arraysize(whatis)) return 0;

    /* Prefer a fast windex database if available. */
    sprintf(whatis, "%s/windex", mandir);

    if ((wf= fopen(whatis, "r")) != nil) {
	searchidx= searchwindex;
    } else {
	/* Use a classic whatis database. */
	sprintf(whatis, "%s/whatis", mandir);

	if ((wf= fopen(whatis, "r")) == nil) return 0;
	searchidx= searchwhatis;
    }

    rsp= 0;
    while (!rsp && (rsw= (*searchidx)(wf, title, &wpage, &wsection)) == 1) {
	if (!member(wsection, section)) continue;

	/* When looking for getc(1S) we try:
	 *	cat1s/getc.1s
	 *	cat1s/getc.1s.Z
	 *	man1s/getc.1s
	 *	man1s/getc.1s.Z
	 *	sman1s/getc.1s
	 *	sman1s/getc.1s.Z
	 *	cat1/getc.1s
	 *	cat1/getc.1s.Z
	 *	man1/getc.1s
	 *	man1/getc.1s.Z
	 */

	if (strlen(mandir) + 2 * strlen(wsection) + strlen(wpage)
		    + 10 > arraysize(pagename))
	    continue;

	sp= searchN;
	ntries= arraysize(searchN);
	do {
	    if (sp->ptype <= CATZ && rofftype != NROFF)
		continue;

	    sprintf(pagename, sp->pathfmt,
		mandir, wsection, wpage, wsection);

	    rsp= showpage(pagename, sp->ptype,
		(SEC9SPECIAL && strcmp(wsection, "9") == 0) ? "-mnx" : "-man");
	} while (sp++, !rsp && --ntries != 0);

	if (all) rsp= 0;
    }
    if (rsw < 0 && ferror(wf)) {
	fprintf(stderr, "man: %s: %s\n", whatis, strerror(errno));
	exit(1);
    }
    fclose(wf);
    return rsp;
}

int trysubmandir(char *mandir, char *title, char *section)
/* Search the subdirectories of this manual directory for whatis files, they
 * may have manual pages that override the ones in the major directory.
 */
{
    char submandir[1024];
    DIR *md;
    struct dirent *entry;

    if ((md= opendir(mandir)) == nil) return 0;

    while ((entry= readdir(md)) != nil) {
	if (strcmp(entry->d_name, ".") == 0
	    || strcmp(entry->d_name, "..") == 0) continue;
	if ((strncmp(entry->d_name, "man", 3) == 0
	    || strncmp(entry->d_name, "cat", 3) == 0)
	    && between('0', entry->d_name[3], '9')) continue;

	if (strlen(mandir) + 1 + strlen(entry->d_name) + 1
		    > arraysize(submandir)) continue;

	sprintf(submandir, "%s/%s", mandir, entry->d_name);

	if (trymandir(submandir, title, section) && !all) {
	    closedir(md);
	    return 1;
	}
    }
    closedir(md);

    return 0;
}

void searchmanpath(char *title, char *section)
/* Search the manual path for a manual page describing "title." */
{
    char mandir[1024];
    char *pp= manpath, *pd;

    for (;;) {
	while (*pp != 0 && *pp == ':') pp++;

	if (*pp == 0) break;

	pd= mandir;
	while (*pp != 0 && *pp != ':') {
	    if (pd < arraylimit(mandir)) *pd++= *pp;
	    pp++;
	}
	if (pd == arraylimit(mandir)) continue;		/* forget it */

	*pd= 0;
	if (trysubmandir(mandir, title, section) && !all) break;
	if (trymandir(mandir, title, section) && !all) break;
    }
}

void usage(void)
{
    fprintf(stderr, "Usage: man -[antfkq] [-M path] [-s section] title ...\n");
    exit(1);
}

int main(int argc, char **argv)
{
    char *title, *section= ALL;
    int i;
    int nomoreopt= 0;
    char *opt;

    if ((pager= getenv("PAGER")) == nil) pager= PAGER;
    if ((manpath= getenv("MANPATH")) == nil) manpath= MANPATH;
    tty= isatty(1);

    i= 1;
    do {
	while (i < argc && argv[i][0] == '-' && !nomoreopt) {
	    opt= argv[i++]+1;
	    if (opt[0] == '-' && opt[1] == 0) {
		nomoreopt= 1;
		break;
	    }
	    while (*opt != 0) {
		switch (*opt++) {
		case 'a':
		    all= 1;
		    break;
		case 'f':
		    whatis= 1;
		    break;
		case 'k':
		    apropos= 1;
		    break;
		case 'q':
		    quiet= 1;
		    break;
		case 'n':
		    rofftype= NROFF;
		    apropos= whatis= 0;
		    break;
		case 't':
		    rofftype= TROFF;
		    apropos= whatis= 0;
		    break;
		case 's':
		    if (*opt == 0) {
			if (i == argc) usage();
			section= argv[i++];
		    } else {
			section= opt;
			opt= "";
		    }
		    break;
		case 'M':
		    if (*opt == 0) {
			if (i == argc) usage();
			manpath= argv[i++];
		    } else {
			manpath= opt;
			opt= "";
		    }
		    break;
		default:
		    usage();
		}
	    }
	}

	if (i >= argc) usage();

	if (between('0', argv[i][0], '9') && argv[i][1] == 0) {
	    /* Allow single digit section designations. */
	    section= argv[i++];
	}
	if (i == argc) usage();

	title= argv[i++];

	if (whatis || apropos) {
	    keyword(title);
	} else {
	    shown= 0;
	    searchmanpath(title, section);

	    if (!shown) (void) showpage(title, MAN, "-man");

	    if (!shown) {
		if (!quiet) {
		    fprintf(stderr,
			"man: no manual on %s\n",
			title);
		}
		exit(1);
	    }
	}
    } while (i < argc);

    return 0;
}
