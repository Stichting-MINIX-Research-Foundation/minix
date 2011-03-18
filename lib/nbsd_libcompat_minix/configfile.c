/*	config_read(), _delete(), _length() - Generic config file routines.
 *							Author: Kees J. Bot
 *								5 Jun 1999
 */
#define nil ((void*)0)
#if __minix_vmd
#include <minix/stubs.h>
#else
#define fstat _fstat
#define stat _stat
#endif
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#if __minix_vmd
#include <minix/asciictype.h>
#else
#include <ctype.h>
#endif
#define _c /* not const */
#include <configfile.h>

typedef struct configfile {	/* List of (included) configuration files. */
	struct configfile *next;	/* A list indeed. */
	time_t		ctime;		/* Last changed time, -1 if no file. */
	char		name[1];	/* File name. */
} configfile_t;

/* Size of a configfile_t given a file name of length 'len'. */
#define configfilesize(len)	(offsetof(configfile_t, name) + 1 + (len))

typedef struct firstconfig {	/* First file and first word share a slot. */
	configfile_t	*filelist;
	char		new;		/* Set when created. */
	config_t	config1;
} firstconfig_t;

/* Size of a config_t given a word of lenght 'len'.  Same for firstconfig_t. */
#define config0size()		(offsetof(config_t, word))
#define configsize(len)		(config0size() + 1 + (len))
#define firstconfigsize(len)	\
			(offsetof(firstconfig_t, config1) + configsize(len))

/* Translate address of first config word to enclosing firstconfig_t and vv. */
#define cfg2fcfg(p)	\
    ((firstconfig_t *) ((char *) (p) - offsetof(firstconfig_t, config1)))
#define fcfg2cfg(p)	(&(p)->config1)

/* Variables used while building data. */
static configfile_t *c_files;		/* List of (included) config files. */
static int c_flags;			/* Flags argument of config_read(). */
static FILE *c_fp;			/* Current open file. */
static char *c_file;			/* Current open file name. */
static unsigned c_line;			/* Current line number. */
static int c;				/* Next character. */

static void *allocate(void *mem, size_t size)
/* Like realloc(), but checked. */
{
    if ((mem= realloc(mem, size)) == nil) {
	fprintf(stderr, "\"%s\", line %u: Out of memory\n", c_file, c_line);
	exit(1);
    }
    return mem;
}

#define deallocate(mem)	free(mem)

static void delete_filelist(configfile_t *cfgf)
/* Delete configuration file list. */
{
    void *junk;

    while (cfgf != nil) {
	junk= cfgf;
	cfgf= cfgf->next;
	deallocate(junk);
    }
}

static void delete_config(config_t *cfg)
/* Delete configuration file data. */
{
    config_t *next, *list, *junk;

    next= cfg;
    list= nil;
    for (;;) {
	if (next != nil) {
	    /* Push the 'next' chain in reverse on the 'list' chain, putting
	     * a leaf cell (next == nil) on top of 'list'.
	     */
	    junk= next;
	    next= next->next;
	    junk->next= list;
	    list= junk;
	} else
	if (list != nil) {
	    /* Delete the leaf cell.  If it has a sublist then that becomes
	     * the 'next' chain.
	     */
	    junk= list;
	    next= list->list;
	    list= list->next;
	    deallocate(junk);
	} else {
	    /* Both chains are gone. */
	    break;
	}
    }
}

void config_delete(config_t *cfg1)
/* Delete configuration file data, being careful with the odd first one. */
{
    firstconfig_t *fcfg= cfg2fcfg(cfg1);

    delete_filelist(fcfg->filelist);
    delete_config(fcfg->config1.next);
    delete_config(fcfg->config1.list);
    deallocate(fcfg);
}

static void nextc(void)
/* Read the next character of the current file into 'c'. */
{
    if (c == '\n') c_line++;
    c= getc(c_fp);
    if (c == EOF && ferror(c_fp)) {
	fprintf(stderr, "\"%s\", line %u: %s\n",
	    c_file, c_line, strerror(errno));
	exit(1);
    }
}

static void skipwhite(void)
/* Skip whitespace and comments. */
{
    while (isspace(c)) {
	nextc();
	if (c == '#') {
	    do nextc(); while (c != EOF && c != '\n');
	}
    }
}

static void parse_err(void)
/* Tell user that you can't parse past the current character. */
{
    char sc[2];

    sc[0]= c;
    sc[1]= 0;
    fprintf(stderr, "\"%s\", line %u: parse error at '%s'\n",
	c_file, c_line, c == EOF ? "EOF" : sc);
    exit(1);
}

static config_t *read_word(void)
/* Read a word or string. */
{
    config_t *w;
    size_t i, len;
    int q;
    static char SPECIAL[] = "!#$%&*+-./:<=>?[\\]^_|~";

    i= 0;
    len= 32;
    w= allocate(nil, configsize(32));
    w->next= nil;
    w->list= nil;
    w->file= c_file;
    w->line= c_line;
    w->flags= 0;

    /* Is it a quoted string? */
    if (c == '\'' || c == '"') {
	q= c;	/* yes */
	nextc();
    } else {
	q= -1;	/* no */
    }

    for (;;) {
	if (i == len) {
	    len+= 32;
	    w= allocate(w, configsize(len));
	}

	if (q == -1) {
	    /* A word consists of letters, numbers and a few special chars. */
	    if (!isalnum(c) && c < 0x80 && strchr(SPECIAL, c) == nil) break;
	} else {
	    /* Strings are made up of anything except newlines. */
	    if (c == EOF || c == '\n') {
		fprintf(stderr,
		    "\"%s\", line %u: string at line %u not closed\n",
		    c_file, c_line, w->line);
		exit(1);
		break;
	    }
	    if (c == q) {	/* Closing quote? */
		nextc();
		break;
	    }
	}

	if (c != '\\') {	/* Simply add non-escapes. */
	    w->word[i++]= c;
	    nextc();
	} else {		/* Interpret an escape. */
	    nextc();
	    if (isspace(c)) {
		skipwhite();
		continue;
	    }

	    if (c_flags & CFG_ESCAPED) {
		w->word[i++]= '\\';	/* Keep the \ for the caller. */
		if (i == len) {
		    len+= 32;
		    w= allocate(w, configsize(len));
		}
		w->flags |= CFG_ESCAPED;
	    }

	    if (isdigit(c)) {		/* Octal escape */
		int n= 3;
		int d= 0;

		do {
		    d= d * 010 + (c - '0');
		    nextc();
		} while (--n > 0 && isdigit(c));
		w->word[i++]= d;
	    } else
	    if (c == 'x' || c == 'X') {	/* Hex escape */
		int n= 2;
		int d= 0;

		nextc();
		if (!isxdigit(c)) {
		    fprintf(stderr, "\"%s\", line %u: bad hex escape\n",
			c_file, c_line);
		    exit(1);
		}
		do {
		    d= d * 0x10 + (islower(c) ? (c - 'a' + 0xa) :
				    isupper(c) ? (c - 'A' + 0xA) :
				    (c - '0'));
		    nextc();
		} while (--n > 0 && isxdigit(c));
		w->word[i++]= d;
	    } else {
		switch (c) {
		case 'a':	c= '\a';	break;
		case 'b':	c= '\b';	break;
		case 'e':	c= '\033';	break;
		case 'f':	c= '\f';	break;
		case 'n':	c= '\n';	break;
		case 'r':	c= '\r';	break;
		case 's':	c= ' ';		break;
		case 't':	c= '\t';	break;
		case 'v':	c= '\v';	break;
		default:	/* Anything else is kept as-is. */;
		}
		w->word[i++]= c;
		nextc();
	    }
	}
    }
    w->word[i]= 0;
    if (q != -1) {
	w->flags |= CFG_STRING;
    } else {
	int f;
	char *end;
	static char base[]= { 0, 010, 10, 0x10 };

	if (i == 0) parse_err();

	/* Can the word be used as a number? */
	for (f= 0; f < 4; f++) {
	    (void) strtol(w->word, &end, base[f]);
	    if (*end == 0) w->flags |= 1 << (f + 0);
	    (void) strtoul(w->word, &end, base[f]);
	    if (*end == 0) w->flags |= 1 << (f + 4);
	}
    }
    return allocate(w, configsize(i));
}

static config_t *read_file(const char *file);
static config_t *read_list(void);

static config_t *read_line(void)
/* Read and return one line of the config file. */
{
    config_t *cline, **pcline, *clist;

    cline= nil;
    pcline= &cline;

    for (;;) {
	skipwhite();

	if (c == EOF || c == '}') {
if(0)	    if (cline != nil) parse_err();
	    break;
	} else
	if (c == ';') {
	    nextc();
	    if (cline != nil) break;
	} else
	if (cline != nil && c == '{') {
	    /* A sublist. */
	    nextc();
	    clist= allocate(nil, config0size());
	    clist->next= nil;
	    clist->file= c_file;
	    clist->line= c_line;
	    clist->list= read_list();
	    clist->flags= CFG_SUBLIST;
	    *pcline= clist;
	    pcline= &clist->next;
	    if (c != '}') parse_err();
	    nextc();
	} else {
	    *pcline= read_word();
	    pcline= &(*pcline)->next;
	}
    }
    return cline;
}

static config_t *read_list(void)
/* Read and return a list of config file commands. */
{
    config_t *clist, **pclist, *cline;

    clist= nil;
    pclist= &clist;

    while ((cline= read_line()) != nil) {
	if (strcmp(cline->word, "include") == 0) {
	    config_t *file= cline->next;
	    if (file == nil || file->next != nil || !config_isatom(file)) {
		fprintf(stderr,
		    "\"%s\", line %u: 'include' command requires an argument\n",
		    c_file, cline->line);
		exit(1);
	    }
	    if (file->flags & CFG_ESCAPED) {
		char *p, *q;
		p= q= file->word;
		for (;;) {
		    if ((*q = *p) == '\\') *q = *++p;
		    if (*q == 0) break;
		    p++;
		    q++;
		}
	    }
	    file= read_file(file->word);
	    delete_config(cline);
	    *pclist= file;
	    while (*pclist != nil) pclist= &(*pclist)->next;
	} else {
	    config_t *cfg= allocate(nil, config0size());
	    cfg->next= nil;
	    cfg->list= cline;
	    cfg->file= cline->file;
	    cfg->line= cline->line;
	    cfg->flags= CFG_SUBLIST;
	    *pclist= cfg;
	    pclist= &cfg->next;
	}
    }
    return clist;
}

static config_t *read_file(const char *file)
/* Read and return a configuration file. */
{
    configfile_t *cfgf;
    config_t *cfg;
    struct stat st;
    FILE *old_fp;	/* old_* variables store current file context. */
    char *old_file;
    unsigned old_line;
    int old_c;
    size_t n;
    char *slash;

    old_fp= c_fp;
    old_file= c_file;
    old_line= c_line;
    old_c= c;

    n= 0;
    if (file[0] != '/' && old_file != nil
			&& (slash= strrchr(old_file, '/')) != nil) {
	n= slash - old_file + 1;
    }
    cfgf= allocate(nil, configfilesize(n + strlen(file)));
    memcpy(cfgf->name, old_file, n);
    strcpy(cfgf->name + n, file);
    cfgf->next= c_files;
    c_files= cfgf;

    c_file= cfgf->name;
    c_line= 0;

    if ((c_fp= fopen(file, "r")) == nil || fstat(fileno(c_fp), &st) < 0) {
	if (errno != ENOENT) {
	    fprintf(stderr, "\"%s\", line 1: %s\n", file, strerror(errno));
	    exit(1);
	}
	cfgf->ctime= -1;
	c= EOF;
    } else {
	cfgf->ctime= st.st_ctime;
	c= '\n';
    }

    cfg= read_list();
    if (c != EOF) parse_err();

    if (c_fp != nil) fclose(c_fp);
    c_fp= old_fp;
    c_file= old_file;
    c_line= old_line;
    c= old_c;
    return cfg;
}

config_t *config_read(const char *file, int flags, config_t *cfg)
/* Read and parse a configuration file. */
{
    if (cfg != nil) {
	/* First check if any of the involved files has changed. */
	firstconfig_t *fcfg;
	configfile_t *cfgf;
	struct stat st;

	fcfg= cfg2fcfg(cfg);
	for (cfgf= fcfg->filelist; cfgf != nil; cfgf= cfgf->next) {
	    if (stat(cfgf->name, &st) < 0) {
		if (errno != ENOENT) break;
		st.st_ctime= -1;
	    }
	    if (st.st_ctime != cfgf->ctime) break;
	}

	if (cfgf == nil) return cfg;	/* Everything as it was. */
	config_delete(cfg);		/* Otherwise delete and reread. */
    }

    errno= 0;
    c_files= nil;
    c_flags= flags;
    cfg= read_file(file);

    if (cfg != nil) {
	/* Change first word to have a hidden pointer to a file list. */
	size_t len= strlen(cfg->word);
	firstconfig_t *fcfg;

	fcfg= allocate(cfg, firstconfigsize(len));
	memmove(&fcfg->config1, fcfg, configsize(len));
	fcfg->filelist= c_files;
	fcfg->new= 1;
	return fcfg2cfg(fcfg);
    }
    /* Couldn't read (errno != 0) of nothing read (errno == 0). */
    delete_filelist(c_files);
    delete_config(cfg);
    return nil;
}

int config_renewed(config_t *cfg)
{
    int new;

    if (cfg == nil) {
	new= 1;
    } else {
	new= cfg2fcfg(cfg)->new;
	cfg2fcfg(cfg)->new= 0;
    }
    return new;
}

size_t config_length(config_t *cfg)
/* Count the number of items on a list. */
{
    size_t n= 0;

    while (cfg != nil) {
	n++;
	cfg= cfg->next;
    }
    return n;
}

#if TEST
#include <unistd.h>

static void print_list(int indent, config_t *cfg);

static void print_words(int indent, config_t *cfg)
{
    while (cfg != nil) {
	if (config_isatom(cfg)) {
	    if (config_isstring(cfg)) fputc('"', stdout);
	    printf("%s", cfg->word);
	    if (config_isstring(cfg)) fputc('"', stdout);
	} else {
	    printf("{\n");
	    print_list(indent+4, cfg->list);
	    printf("%*s}", indent, "");
	}
	cfg= cfg->next;
	if (cfg != nil) fputc(' ', stdout);
    }
    printf(";\n");
}

static void print_list(int indent, config_t *cfg)
{
    while (cfg != nil) {
	if (!config_issub(cfg)) {
	    fprintf(stderr, "Cell at \"%s\", line %u is not a sublist\n");
	    break;
	}
	printf("%*s", indent, "");
	print_words(indent, cfg->list);
	cfg= cfg->next;
    }
}

static void print_config(config_t *cfg)
{
    if (!config_renewed(cfg)) {
	printf("# Config didn't change\n");
    } else {
	print_list(0, cfg);
    }
}

int main(int argc, char **argv)
{
    config_t *cfg;
    int c;

    if (argc != 2) {
	fprintf(stderr, "One config file name please\n");
	exit(1);
    }

    cfg= nil;
    do {
	cfg= config_read(argv[1], CFG_ESCAPED, cfg);
	print_config(cfg);
	if (!isatty(0)) break;
	while ((c= getchar()) != EOF && c != '\n') {}
    } while (c != EOF);
    return 0;
}
#endif /* TEST */
