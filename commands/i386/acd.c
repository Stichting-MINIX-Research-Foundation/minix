/*	acd 1.10 - A compiler driver			Author: Kees J. Bot
 *								7 Jan 1993
 * Needs about 25kw heap + stack.
 */
char version[] = "1.9";

#define nil 0
#define _POSIX_SOURCE	1
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifndef LIB
#define LIB	"/usr/lib"	/* Default library directory. */
#endif

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

char *program;		/* Call name. */

int verbose= 0;		/* -v0: Silent.
			 * -v1: Show abbreviated pass names.
			 * -v2: Show executed UNIX commands.
			 * -v3: Show executed ACD commands.
			 * -v4: Show descr file as it is read.
			 */

int action= 2;		/*   0: An error occured, don't do anything anymore.
			 *   1: (-vn) Do not execute, play-act.
			 *   2: Execute UNIX commands.
			 */

void report(char *label)
{
	if (label == nil || label[0] == 0) {
		fprintf(stderr, "%s: %s\n", program, strerror(errno));
	} else {
		fprintf(stderr, "%s: %s: %s\n",
					program, label, strerror(errno));
	}
	action= 0;
}

void quit(int exit_code);

void fatal(char *label)
{
	report(label);
	quit(-1);
}

size_t heap_chunks= 0;

void *allocate(void *mem, size_t size)
/* Safe malloc/realloc.  (I have heard that one can call realloc with a
 * null first argument with the effect below, but that is of course to
 * ridiculous to believe.)
 */
{
	assert(size > 0);

	if (mem != nil) {
		mem= realloc(mem, size);
	} else {
		mem= malloc(size);
		heap_chunks++;
	}
	if (mem == nil) fatal(nil);
	return mem;
}

void deallocate(void *mem)
{
	if (mem != nil) {
		free(mem);
		heap_chunks--;
	}
}

char *copystr(const char *s)
{
	char *c;
	c= allocate(nil, (strlen(s)+1) * sizeof(*c));
	strcpy(c, s);
	return c;
}

/* Every object, list, letter, or variable, is made with cells. */
typedef struct cell {
	unsigned short	refc;		/* Reference count. */
	char		type;		/* Type of object. */
	unsigned char	letter;		/* Simply a letter. */
	char		*name;		/* Name of a word. */
	struct cell	*hash;		/* Hash chain. */
	struct cell	*car, *cdr;	/* To form lists. */

/* For a word: */
#	define	value	car		/* Value of a variable. */
#	define	base	cdr		/* Base-name in transformations. */
#	define	suffix	cdr		/* Suffix in a treat-as. */
#	define	flags	letter		/* Special flags. */

/* A substitution: */
#	define	subst	car

} cell_t;

typedef enum type {
	CELL,		/* A list cell. */
	STRING,		/* To make a list of characters and substs. */
	SUBST,		/* Variable to substitute. */
	/* Unique objects. */
	LETTER,		/* A letter. */
	WORD,		/* A string collapses to a word. */
	EQUALS,		/* = operator, etc. */
	OPEN,
	CLOSE,
	PLUS,
	MINUS,
	STAR,
	INPUT,
	OUTPUT,
	WHITE,
	COMMENT,
	SEMI,
	EOLN,
	N_TYPES		/* number of different types */
} type_t;

#define is_unique(type) ((type) >= LETTER)

/* Flags on a word. */
#define W_SET		0x01	/* Not undefined, e.g. assigned to. */
#define W_RDONLY	0x02	/* Read only. */
#define W_LOCAL		0x04	/* Local variable, immediate substitution. */
#define W_TEMP		0x08	/* Name of a temporary file, delete on quit. */
#define W_SUFF		0x10	/* Has a suffix set on it. */

void princhar(int c)
/* Print a character, escaped if important to the shell *within* quotes. */
{
	if (strchr("\\'\"<>();~$^&*|{}[]?", c) != nil) fputc('\\', stdout);
	putchar(c);
}

void prinstr(char *s)
/* Print a string, in quotes if the shell might not like it. */
{
	int q= 0;
	char *s2= s;

	while (*s2 != 0)
		if (strchr("~`$^&*()=\\|[]{};'\"<>?", *s2++) != nil) q= 1;

	if (q) fputc('"', stdout);
	while (*s != 0) princhar(*s++);
	if (q) fputc('"', stdout);
}

void prin2(cell_t *p);

void prin1(cell_t *p)
/* Print a cell structure for debugging purposes. */
{
	if (p == nil) {
		printf("(\b(\b()\b)\b)");
		return;
	}

	switch (p->type) {
	case CELL:
		printf("(\b(\b(");
		prin2(p);
		printf(")\b)\b)");
		break;
	case STRING:
		printf("\"\b\"\b\"");
		prin2(p);
		printf("\"\b\"\b\"");
		break;
	case SUBST:
		printf("$\b$\b${%s}", p->subst->name);
		break;
	case LETTER:
		princhar(p->letter);
		break;
	case WORD:
		prinstr(p->name);
		break;
	case EQUALS:
		printf("=\b=\b=");
		break;
	case PLUS:
		printf("+\b+\b+");
		break;
	case MINUS:
		printf("-\b-\b-");
		break;
	case STAR:
		printf("*\b*\b*");
		break;
	case INPUT:
		printf(verbose >= 3 ? "<\b<\b<" : "<");
		break;
	case OUTPUT:
		printf(verbose >= 3 ? ">\b>\b>" : ">");
		break;
	default:
		assert(0);
	}
}

void prin2(cell_t *p)
/* Print a list for debugging purposes. */
{
	while (p != nil && p->type <= STRING) {
		prin1(p->car);

		if (p->type == CELL && p->cdr != nil) fputc(' ', stdout);

		p= p->cdr;
	}
	if (p != nil) prin1(p);		/* Dotted pair? */
}

void prin1n(cell_t *p) { prin1(p); fputc('\n', stdout); }

void prin2n(cell_t *p) { prin2(p); fputc('\n', stdout); }

/* A program is consists of a series of lists at a certain indentation level. */
typedef struct program {
	struct program	*next;
	cell_t		*file;		/* Associated description file. */
	unsigned	indent;		/* Line indentation level. */
	unsigned	lineno;		/* Line number where this is found. */
	cell_t		*line;		/* One line of tokens. */
} program_t;

program_t *pc;		/* Program Counter (what else?) */
program_t *nextpc;	/* Next line to execute. */

cell_t *oldcells;	/* Keep a list of old cells, don't deallocate. */

cell_t *newcell(void)
/* Make a new empty cell. */
{
	cell_t *p;

	if (oldcells != nil) {
		p= oldcells;
		oldcells= p->cdr;
		heap_chunks++;
	} else {
		p= allocate(nil, sizeof(*p));
	}

	p->refc= 0;
	p->type= CELL;
	p->letter= 0;
	p->name= nil;
	p->car= nil;
	p->cdr= nil;
	return p;
}

#define N_CHARS		(1 + (unsigned char) -1)
#define HASHDENSE	0x400

cell_t *oblist[HASHDENSE + N_CHARS + N_TYPES];

unsigned hashfun(cell_t *p)
/* Use a blender on a cell. */
{
	unsigned h;
	char *name;

	switch (p->type) {
	case WORD:
		h= 0;
		name= p->name;
		while (*name != 0) h= (h * 0x1111) + *name++;
		return h % HASHDENSE;
	case LETTER:
		return HASHDENSE + p->letter;
	default:
		return HASHDENSE + N_CHARS + p->type;
	}
}

cell_t *search(cell_t *p, cell_t ***hook)
/* Search for *p, return the one found.  *hook may be used to insert or
 * delete.
 */
{
	cell_t *sp;

	sp= *(*hook= &oblist[hashfun(p)]);

	if (p->type == WORD) {
		/* More than one name per hash slot. */
		int cmp= 0;

		while (sp != nil && (cmp= strcmp(p->name, sp->name)) > 0)
			sp= *(*hook= &sp->hash);

		if (cmp != 0) sp= nil;
	}
	return sp;
}

void dec(cell_t *p)
/* Decrease the number of references to p, if zero delete and recurse. */
{
	if (p == nil || --p->refc > 0) return;

	if (is_unique(p->type)) {
		/* Remove p from the oblist. */
		cell_t *o, **hook;

		o= search(p, &hook);

		if (o == p) {
			/* It's there, remove it. */
			*hook= p->hash;
			p->hash= nil;
		}

		if (p->type == WORD && (p->flags & W_TEMP)) {
			/* A filename to remove. */
			if (verbose >= 2) {
				printf("rm -f ");
				prinstr(p->name);
				fputc('\n', stdout);
			}
			if (unlink(p->name) < 0 && errno != ENOENT)
				report(p->name);
		}
	}
	deallocate(p->name);
	dec(p->car);
	dec(p->cdr);
	p->cdr= oldcells;
	oldcells= p;
	heap_chunks--;
}

cell_t *inc(cell_t *p)
/* Increase the number of references to p. */
{
	cell_t *o, **hook;

	if (p == nil) return nil;

	if (++p->refc > 1 || !is_unique(p->type)) return p;

	/* First appearance, put p on the oblist. */
	o= search(p, &hook);

	if (o == nil) {
		/* Not there yet, add it. */
		p->hash= *hook;
		*hook= p;
	} else {
		/* There is another object already there with the same info. */
		o->refc++;
		dec(p);
		p= o;
	}
	return p;
}

cell_t *go(cell_t *p, cell_t *field)
/* Often happening: You've got p, you want p->field. */
{
	field= inc(field);
	dec(p);
	return field;
}

cell_t *cons(type_t type, cell_t *p)
/* P is to be added to a list (or a string). */
{
	cell_t *l= newcell();
	l->type= type;
	l->refc++;
	l->car= p;
	return l;
}

cell_t *append(type_t type, cell_t *p)
/* P is to be appended to a list (or a string). */
{
	return p == nil || p->type == type ? p : cons(type, p);
}

cell_t *findnword(char *name, size_t n)
/* Find the word with the given name of length n. */
{
	cell_t *w= newcell();
	w->type= WORD;
	w->name= allocate(nil, (n+1) * sizeof(*w->name));
	memcpy(w->name, name, n);
	w->name[n]= 0;
	return inc(w);
}

cell_t *findword(char *name)
/* Find the word with the given null-terminated name. */
{
	return findnword(name, strlen(name));
}

void quit(int exstat)
/* Remove all temporary names, then exit. */
{
	cell_t **op, *p, *v, *b;
	size_t chunks;

	/* Remove cycles, like X = X. */
	for (op= oblist; op < oblist + HASHDENSE; op++) {
		p= *op;
		while (p != nil) {
			if (p->value != nil || p->base != nil) {
				v= p->value;
				b= p->base;
				p->value= nil;
				p->base= nil;
				p= *op;
				dec(v);
				dec(b);
			} else {
				p= p->hash;
			}
		}
	}
	chunks= heap_chunks;

	/* Something may remain on an early quit: tempfiles. */
	for (op= oblist; op < oblist + HASHDENSE; op++) {

		while (*op != nil) { (*op)->refc= 1; dec(*op); }
	}

	if (exstat != -1 && chunks > 0) {
		fprintf(stderr,
			"%s: internal fault: %d chunks still on the heap\n",
						program, chunks);
	}
	exit(exstat);
}

void interrupt(int sig)
{
	signal(sig, interrupt);
	if (verbose >= 2) write(1, "# interrupt\n", 12);
	action= 0;
}

int extalnum(int c)
/* Uppercase, lowercase, digit, underscore or anything non-American. */
{
	return isalnum(c) || c == '_' || c >= 0200;
}

char *descr;		/* Name of current description file. */
FILE *dfp;		/* Open description file. */
int dch;		/* Input character. */
unsigned lineno;	/* Line number in file. */
unsigned indent;	/* Indentation level. */

void getdesc(void)
{
	if (dch == EOF) return;

	if (dch == '\n') { lineno++; indent= 0; }

	if ((dch = getc(dfp)) == EOF && ferror(dfp)) fatal(descr);

	if (dch == 0) {
		fprintf(stderr, "%s: %s is a binary file.\n", program, descr);
		quit(-1);
	}
}

#define E_BASH		0x01	/* Escaped by backslash. */
#define E_QUOTE		0x02	/* Escaped by double quote. */
#define E_SIMPLE	0x04	/* More simple characters? */

cell_t *get_token(void)
/* Read one token from the description file. */
{
	int whitetype= 0;
	static int escape= 0;
	cell_t *tok;
	char *name;
	int n, i;

	if (escape & E_SIMPLE) {
		/* More simple characters?  (Note: performance hack.) */
		if (isalnum(dch)) {
			tok= newcell();
			tok->type= LETTER;
			tok->letter= dch;
			getdesc();
			return inc(tok);
		}
		escape&= ~E_SIMPLE;
	}

	/* Gather whitespace. */
	for (;;) {
		if (dch == '\\' && whitetype == 0) {
			getdesc();
			if (isspace(dch)) {
				/* \ whitespace: remove. */
				do {
					getdesc();
					if (dch == '#' && !(escape & E_QUOTE)) {
						/* \ # comment */
						do
							getdesc();
						while (dch != '\n'
								&& dch != EOF);
					}
				} while (isspace(dch));
				continue;
			}
			escape|= E_BASH;	/* Escaped character. */
		}

		if (escape != 0) break;

		if (dch == '#' && (indent == 0 || whitetype != 0)) {
			/* # Comment. */
			do getdesc(); while (dch != '\n' && dch != EOF);
			whitetype= COMMENT;
			break;
		}

		if (!isspace(dch) || dch == '\n' || dch == EOF) break;

		whitetype= WHITE;

		indent++;
		if (dch == '\t') indent= (indent + 7) & ~7;

		getdesc();
	}

	if (dch == EOF) return nil;

	/* Make a token. */
	tok= newcell();

	if (whitetype != 0) {
		tok->type= whitetype;
		return inc(tok);
	}

	if (!(escape & E_BASH) && dch == '"') {
		getdesc();
		if (!(escape & E_QUOTE)) {
			/* Start of a string, signal this with a string cell. */
			escape|= E_QUOTE;
			tok->type= STRING;
			return inc(tok);
		} else {
			/* End of a string, back to normal mode. */
			escape&= ~E_QUOTE;
			deallocate(tok);
			return get_token();
		}
	}

	if (escape & E_BASH
		|| strchr(escape & E_QUOTE ? "$" : "$=()+-*<>;\n", dch) == nil
	) {
		if (dch == '\n') {
			fprintf(stderr,
				"\"%s\", line %u: missing closing quote\n",
				descr, lineno);
			escape&= ~E_QUOTE;
			action= 0;
		}
		if (escape & E_BASH && dch == 'n') dch= '\n';
		escape&= ~E_BASH;

		/* A simple character. */
		tok->type= LETTER;
		tok->letter= dch;
		getdesc();
		escape|= E_SIMPLE;
		return inc(tok);
	}

	if (dch != '$') {
		/* Single character token. */
		switch (dch) {
		case '=':	tok->type= EQUALS;	break;
		case '(':	tok->type= OPEN;	break;
		case ')':	tok->type= CLOSE;	break;
		case '+':	tok->type= PLUS;	break;
		case '-':	tok->type= MINUS;	break;
		case '*':	tok->type= STAR;	break;
		case '<':	tok->type= INPUT;	break;
		case '>':	tok->type= OUTPUT;	break;
		case ';':	tok->type= SEMI;	break;
		case '\n':	tok->type= EOLN;	break;
		}
		getdesc();
		return inc(tok);
	}

	/* Substitution. */
	getdesc();
	if (dch == EOF || isspace(dch)) {
		fprintf(stderr, "\"%s\", line %u: Word expected after '$'\n",
			descr, lineno);
		action= 0;
		deallocate(tok);
		return get_token();
	}

	name= allocate(nil, (n= 16) * sizeof(*name));
	i= 0;

	if (dch == '{' || dch == '('  /* )} */ ) {
		/* $(X), ${X} */
		int lpar= dch;		/* ( */
		int rpar= lpar == '{' ? '}' : ')';

		for (;;) {
			getdesc();
			if (dch == rpar) { getdesc(); break; }
			if (isspace(dch) || dch == EOF) {
				fprintf(stderr,
				"\"%s\", line %u: $%c unmatched, no '%c'\n",
					descr, lineno, lpar, rpar);
				action= 0;
				break;
			}
			name[i++]= dch;
			if (i == n)
				name= allocate(name, (n*= 2) * sizeof(char));
		}
	} else
	if (extalnum(dch)) {
		/* $X */
		do {
			name[i++]= dch;
			if (i == n)
				name= allocate(name, (n*= 2) * sizeof(char));
			getdesc();
		} while (extalnum(dch));
	} else {
		/* $* */
		name[i++]= dch;
		getdesc();
	}
	name[i++]= 0;
	name= allocate(name, i * sizeof(char));
	tok->type= SUBST;
	tok->subst= newcell();
	tok->subst->type= WORD;
	tok->subst->name= name;
	tok->subst= inc(tok->subst);
	return inc(tok);
}

typedef enum how { SUPERFICIAL, PARTIAL, FULL, EXPLODE, IMPLODE } how_t;

cell_t *explode(cell_t *p, how_t how);

cell_t *get_string(cell_t **pp)
/* Get a string: A series of letters and substs.  Special tokens '=', '+', '-'
 * and '*' are also recognized if on their own.  A finished string is "exploded"
 * to a word if it consists of letters only.
 */
{
	cell_t *p= *pp, *s= nil, **ps= &s;
	int quoted= 0;

	while (p != nil) {
		switch (p->type) {
		case STRING:
			quoted= 1;
			dec(p);
			break;
		case EQUALS:
		case PLUS:
		case MINUS:
		case STAR:
		case SUBST:
		case LETTER:
			*ps= cons(STRING, p);
			ps= &(*ps)->cdr;
			break;
		default:
			goto got_string;
		}
		p= get_token();
	}
    got_string:
	*pp= p;

	/* A single special token must be folded up. */
	if (!quoted && s != nil && s->cdr == nil) {
		switch (s->car->type) {
		case EQUALS:
		case PLUS:
		case MINUS:
		case STAR:
		case SUBST:
			return go(s, s->car);
		}
	}

	/* Go over the string changing '=', '+', '-', '*' to letters. */
	for (p= s; p != nil; p= p->cdr) {
		int c= 0;

		switch (p->car->type) {
		case EQUALS:
			c= '='; break;
		case PLUS:
			c= '+'; break;
		case MINUS:
			c= '-'; break;
		case STAR:
			c= '*'; break;
		}
		if (c != 0) {
			dec(p->car);
			p->car= newcell();
			p->car->type= LETTER;
			p->car->letter= c;
			p->car= inc(p->car);
		}
	}
	return explode(s, SUPERFICIAL);
}

cell_t *get_list(cell_t **pp, type_t stop)
/* Read a series of tokens upto a token of type "stop". */
{
	cell_t *p= *pp, *l= nil, **pl= &l;

	while (p != nil && p->type != stop
				&& !(stop == EOLN && p->type == SEMI)) {
		switch (p->type) {
		case WHITE:
		case COMMENT:
		case SEMI:
		case EOLN:
			dec(p);
			p= get_token();
			break;
		case OPEN:
			/* '(' words ')'. */
			dec(p);
			p= get_token();
			*pl= cons(CELL, get_list(&p, CLOSE));
			pl= &(*pl)->cdr;
			dec(p);
			p= get_token();
			break;
		case CLOSE:
			/* Unexpected closing parenthesis. (*/
			fprintf(stderr, "\"%s\", line %u: unmatched ')'\n",
				descr, lineno);
			action= 0;
			dec(p);
			p= get_token();
			break;
		case INPUT:
		case OUTPUT:
			*pl= cons(CELL, p);
			pl= &(*pl)->cdr;
			p= get_token();
			break;
		case STRING:
		case EQUALS:
		case PLUS:
		case MINUS:
		case STAR:
		case LETTER:
		case SUBST:
			*pl= cons(CELL, get_string(&p));
			pl= &(*pl)->cdr;
			break;
		default:
			assert(0);
		}
	}

	if (p == nil && stop == CLOSE) {
		/* Couldn't get the closing parenthesis. */
		fprintf(stderr, "\"%s\", lines %u-%u: unmatched '('\n",	/*)*/
			descr, pc->lineno, lineno);
		action= 0;
	}
	*pp= p;
	return l;
}

program_t *get_line(cell_t *file)
{
	program_t *l;
	cell_t *p;
	static keep_indent= 0;
	static unsigned old_indent= 0;

	/* Skip leading whitespace to determine the indentation level. */
	indent= 0;
	while ((p= get_token()) != nil && p->type == WHITE) dec(p);

	if (p == nil) return nil;		/* EOF */

	if (p->type == EOLN) indent= old_indent;	/* Empty line. */

	/* Make a program line. */
	pc= l= allocate(nil, sizeof(*l));

	l->next= nil;
	l->file= inc(file);
	l->indent= keep_indent ? old_indent : indent;
	l->lineno= lineno;

	l->line= get_list(&p, EOLN);

	/* If the line ended in a semicolon then keep the indentation level. */
	keep_indent= (p != nil && p->type == SEMI);
	old_indent= l->indent;

	dec(p);

	if (verbose >= 4) {
		if (l->line == nil)
			fputc('\n', stdout);
		else {
			printf("%*s", (int) l->indent, "");
			prin2n(l->line);
		}
	}
	return l;
}

program_t *get_prog(void)
/* Read the description file into core. */
{
	cell_t *file;
	program_t *prog, **ppg= &prog;

	descr= copystr(descr);

	if (descr[0] == '-' && descr[1] == 0) {
		/* -descr -: Read from standard input. */
		deallocate(descr);
		descr= copystr("stdin");
		dfp= stdin;
	} else {
		char *d= descr;

		if (*d == '.' && *++d == '.') d++;
		if (*d != '/') {
			/* -descr name: Read /usr/lib/<name>/descr. */

			d= allocate(nil, sizeof(LIB) +
					(strlen(descr) + 7) * sizeof(*d));
			sprintf(d, "%s/%s/descr", LIB, descr);
			deallocate(descr);
			descr= d;
		}
		if ((dfp= fopen(descr, "r")) == nil) fatal(descr);
	}
	file= findword(descr);
	deallocate(descr);
	descr= file->name;

	/* Preread the first character. */
	dch= 0;
	lineno= 1;
	indent= 0;
	getdesc();

	while ((*ppg= get_line(file)) != nil) ppg= &(*ppg)->next;

	if (dfp != stdin) (void) fclose(dfp);
	dec(file);

	return prog;
}

void makenames(cell_t ***ppr, cell_t *s, char **name, size_t i, size_t *n)
/* Turn a string of letters and lists into words.  A list denotes a choice
 * between several paths, like a search on $PATH.
 */
{
	cell_t *p, *q;
	size_t len;

	/* Simply add letters, skip empty lists. */
	while (s != nil && (s->car == nil || s->car->type == LETTER)) {
		if (s->car != nil) {
			if (i == *n) *name= allocate(*name,
						(*n *= 2) * sizeof(**name));
			(*name)[i++]= s->car->letter;
		}
		s= s->cdr;
	}

	/* If the end is reached then make a word out of the result. */
	if (s == nil) {
		**ppr= cons(CELL, findnword(*name, i));
		*ppr= &(**ppr)->cdr;
		return;
	}

	/* Elements of a list must be tried one by one. */
	p= s->car;
	s= s->cdr;

	while (p != nil) {
		if (p->type == WORD) {
			q= p; p= nil;
		} else {
			assert(p->type == CELL);
			q= p->car; p= p->cdr;
			assert(q != nil);
			assert(q->type == WORD);
		}
		len= strlen(q->name);
		if (i + len > *n) *name= allocate(*name,
					(*n += i + len) * sizeof(**name));
		memcpy(*name + i, q->name, len);

		makenames(ppr, s, name, i+len, n);
	}
}

int constant(cell_t *p)
/* See if a string has been partially evaluated to a constant so that it
 * can be imploded to a word.
 */
{
	while (p != nil) {
		switch (p->type) {
		case CELL:
		case STRING:
			if (!constant(p->car)) return 0;
			p= p->cdr;
			break;
		case SUBST:
			return 0;
		default:
			return 1;
		}
	}
	return 1;
}

cell_t *evaluate(cell_t *p, how_t how);

cell_t *explode(cell_t *s, how_t how)
/* Explode a string with several choices to just one list of choices. */
{
	cell_t *t, *r= nil, **pr= &r;
	size_t i, n;
	char *name;
	struct stat st;

	if (how >= PARTIAL) {
		/* Evaluate the string, expanding substitutions. */
		while (s != nil) {
			assert(s->type == STRING);
			t= inc(s->car);
			s= go(s, s->cdr);

			t= evaluate(t, how == IMPLODE ? EXPLODE : how);

			/* A list of one element becomes that element. */
			if (t != nil && t->type == CELL && t->cdr == nil)
				t= go(t, t->car);

			/* Append the result, trying to flatten it. */
			*pr= t;

			/* Find the end of what has just been added. */
			while ((*pr) != nil) {
				*pr= append(STRING, *pr);
				pr= &(*pr)->cdr;
			}
		}
		s= r;
	}

	/* Is the result a simple string of constants? */
	if (how <= PARTIAL && !constant(s)) return s;

	/* Explode the string to all possible choices, by now the string is
	 * a series of characters, words and lists of words.
	 */
	r= nil; pr= &r;
	name= allocate(nil, (n= 16) * sizeof(char));
	i= 0;

	makenames(&pr, s, &name, i, &n);
	deallocate(name);
	assert(r != nil);
	dec(s);
	s= r;

	/* "How" may specify that a choice must be made. */
	if (how == IMPLODE) {
		if (s->cdr != nil) {
			/* More than one choice, find the file. */
			do {
				assert(s->car->type == WORD);
				if (stat(s->car->name, &st) >= 0)
					return go(r, s->car);	/* Found. */
			} while ((s= s->cdr) != nil);
		}
		/* The first name is the default if nothing is found. */
		return go(r, r->car);
	}

	/* If the result is a list of one word then return that word, otherwise
	 * turn it into a string again unless this explode has been called
	 * by another explode.  (Exploding a string inside a string, the joys
	 * of recursion.)
	 */
	if (s->cdr == nil) return go(s, s->car);

	return how >= EXPLODE ? s : cons(STRING, s);
}

void modify(cell_t **pp, cell_t *p, type_t mode)
/* Add or remove the element p from the list *pp. */
{
	while (*pp != nil) {
		*pp= append(CELL, *pp);

		if ((*pp)->car == p) {
			/* Found it, if adding then exit, else remove. */
			if (mode == PLUS) break;
			*pp= go(*pp, (*pp)->cdr);
		} else
			pp= &(*pp)->cdr;
	}

	if (*pp == nil && mode == PLUS) {
		/* Not found, add it. */
		*pp= cons(CELL, p);
	} else
		dec(p);
}

int tainted(cell_t *p)
/* A variable is tainted (must be substituted) if either it is marked as a
 * local variable, or some subst in its value is.
 */
{
	if (p == nil) return 0;

	switch (p->type) {
	case CELL:
	case STRING:
		return tainted(p->car) || tainted(p->cdr);
	case SUBST:
		return p->subst->flags & W_LOCAL || tainted(p->subst->value);
	default:
		return 0;
	}
}

cell_t *evaluate(cell_t *p, how_t how)
/* Evaluate an expression, usually the right hand side of an assignment. */
{
	cell_t *q, *t, *r= nil, **pr= &r;
	type_t mode;

	if (p == nil) return nil;

	switch (p->type) {
	case CELL:
		break;	/* see below */
	case STRING:
		return explode(p, how);
	case SUBST:
		if (how >= FULL || tainted(p))
			p= evaluate(go(p, p->subst->value), how);
		return p;
	case EQUALS:
		fprintf(stderr,
			"\"%s\", line %u: Can't do nested assignments\n",
			descr, pc->lineno);
		action= 0;
		dec(p);
		return nil;
	case LETTER:
	case WORD:
	case INPUT:
	case OUTPUT:
	case PLUS:
	case MINUS:
		return p;
	default:
		assert(0);
	}

	/* It's a list, see if there is a '*' there forcing a full expansion,
	 * or a '+' or '-' forcing an implosive expansion.  (Yeah, right.)
	 * Otherwise evaluate each element.
	 */
	q = inc(p);
	while (p != nil) {
		if ((t= p->car) != nil) {
			if (t->type == STAR) {
				if (how < FULL) how= FULL;
				dec(q);
				*pr= evaluate(go(p, p->cdr), how);
				return r;
			}
			if (how>=FULL && (t->type == PLUS || t->type == MINUS))
				break;
		}

		t= evaluate(inc(t), how);
		assert(p->type == CELL);
		p= go(p, p->cdr);

		if (how >= FULL) {
			/* Flatten the list. */
			*pr= t;
		} else {
			/* Keep the nested list structure. */
			*pr= cons(CELL, t);
		}

		/* Find the end of what has just been added. */
		while ((*pr) != nil) {
			*pr= append(CELL, *pr);
			pr= &(*pr)->cdr;
		}
	}

	if (p == nil) {
		/* No PLUS or MINUS: done. */
		dec(q);
		return r;
	}

	/* A PLUS or MINUS, reevaluate the original list implosively. */
	if (how < IMPLODE) {
		dec(r);
		dec(p);
		return evaluate(q, IMPLODE);
	}
	dec(q);

	/* Execute the PLUSes and MINUSes. */
	while (p != nil) {
		t= inc(p->car);
		p= go(p, p->cdr);

		if (t != nil && (t->type == PLUS || t->type == MINUS)) {
			/* Change the add/subtract mode. */
			mode= t->type;
			dec(t);
			continue;
		}

		t= evaluate(t, IMPLODE);

		/* Add or remove all elements of t to/from r. */
		while (t != nil) {
			if (t->type == CELL) {
				modify(&r, inc(t->car), mode);
			} else {
				modify(&r, t, mode);
				break;
			}
			t= go(t, t->cdr);
		}
	}
	return r;
}

/* An ACD program can be in three phases: Initialization (the first run
 * of the program), argument scanning, and compilation.
 */
typedef enum phase { INIT, SCAN, COMPILE } phase_t;

phase_t phase;

typedef struct rule {		/* Transformation rule. */
	struct rule	*next;
	char		type;		/* arg, transform, combine */
	char		flags;
	unsigned short	npaths;		/* Number of paths running through. */
#	define	match	from		/* Arg matching strings. */
	cell_t		*from;		/* Transformation source suffixe(s) */
	cell_t		*to;		/* Destination suffix. */
	cell_t		*wait;		/* Files waiting to be transformed. */
	program_t	*prog;		/* Program to execute. */
	struct rule	*path;		/* Transformation path. */
} rule_t;

typedef enum ruletype { ARG, PREFER, TRANSFORM, COMBINE } ruletype_t;

#define R_PREFER	0x01		/* A preferred transformation. */

rule_t *rules= nil;

void newrule(ruletype_t type, cell_t *from, cell_t *to)
/* Make a new rule cell. */
{
	rule_t *r= nil, **pr= &rules;

	/* See if there is a rule with the same suffixes, probably a matching
	 * transform and prefer, or a re-execution of the same arg command.
	 */
	while ((r= *pr) != nil) {
		if (r->from == from && r->to == to) break;
		pr= &r->next;
	}

	if (*pr == nil) {
		/* Add a new rule. */
		*pr= r= allocate(nil, sizeof(*r));

		r->next= nil;
		r->type= type;
		r->flags= 0;
		r->from= r->to= r->wait= nil;
		r->path= nil;
	}
	if (type == TRANSFORM) r->type= TRANSFORM;
	if (type == PREFER) r->flags|= R_PREFER;
	if (type != PREFER) r->prog= pc;
	dec(r->from); r->from= from;
	dec(r->to); r->to= to;
}

int talk(void)
/* True if verbose and if so indent what is to come. */
{
	if (verbose < 3) return 0;
	printf("%*s", (int) pc->indent, "");
	return 1;
}

void unix_exec(cell_t *c)
/* Execute the list of words p as a UNIX command. */
{
	cell_t *v, *a;
	int fd[2];
	int *pf;
	char **argv;
	int i, n;
	int r, pid, status;

	if (action == 0) return;	/* Error mode. */

	if (talk() || verbose >= 2) prin2n(c);

	fd[0]= fd[1]= -1;

	argv= allocate(nil, (n= 16) * sizeof(*argv));
	i= 0;

	/* Gather argv[] and scan for I/O redirection. */
	for (v= c; v != nil; v= v->cdr) {
		a= v->car;
		pf= nil;
		if (a->type == INPUT) pf= &fd[0];
		if (a->type == OUTPUT) pf= &fd[1];

		if (pf == nil) {
			/* An argument. */
			argv[i++]= a->name;
			if (i==n) argv= allocate(argv, (n*= 2) * sizeof(*argv));
			continue;
		}
		/* I/O redirection. */
		if ((v= v->cdr) == nil || (a= v->car)->type != WORD) {
			fprintf(stderr,
			"\"%s\", line %u: I/O redirection without a file\n",
				descr, pc->lineno);
			action= 0;
			if (v == nil) break;
		}
		if (*pf >= 0) close(*pf);

		if (action >= 2
			&& (*pf= open(a->name, pf == &fd[0] ? O_RDONLY
				: O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0
		) {
			report(a->name);
			action= 0;
		}
	}
	argv[i]= nil;

	if (i >= 0 && action > 0 && verbose == 1) {
		char *name= strrchr(argv[0], '/');

		if (name == nil) name= argv[0]; else name++;

		printf("%s\n", name);
	}
	if (i >= 0 && action >= 2) {
		/* Really execute the command. */
		fflush(stdout);
		switch (pid= fork()) {
		case -1:
			fatal("fork()");
		case 0:
			if (fd[0] >= 0) { dup2(fd[0], 0); close(fd[0]); }
			if (fd[1] >= 0) { dup2(fd[1], 1); close(fd[1]); }
			execvp(argv[0], argv);
			report(argv[0]);
			exit(-1);
		}
	}
	if (fd[0] >= 0) close(fd[0]);
	if (fd[1] >= 0) close(fd[1]);

	if (i >= 0 && action >= 2) {
		/* Wait for the command to terminate. */
		while ((r= wait(&status)) != pid && (r >= 0 || errno == EINTR));

		if (status != 0) {
			int sig= WTERMSIG(status);

			if (!WIFEXITED(status)
					&& sig != SIGINT && sig != SIGPIPE) {
				fprintf(stderr, "%s: %s: Signal %d%s\n",
					program, argv[0], sig,
					status & 0x80 ? " - core dumped" : "");
			}
			action= 0;
		}
	}
	deallocate(argv);
}

/* Special read-only variables ($*) and lists. */
cell_t *V_star, **pV_star;
cell_t *L_files, **pL_files= &L_files;
cell_t *V_in, *V_out, *V_stop, *L_args, *L_predef;

typedef enum exec { DOIT, DONT } exec_t;

void execute(exec_t how, unsigned indent);

int equal(cell_t *p, cell_t *q)
/* Two lists are equal if they contain each others elements. */
{
	cell_t *t, *m1, *m2;

	t= inc(newcell());
	t->cdr= inc(newcell());
	t->cdr->cdr= inc(newcell());
	t->cdr->car= newcell();
	t->cdr->car->type= MINUS;
	t->cdr->car= inc(t->cdr->car);

	/* Compute p - q. */
	t->car= inc(p);
	t->cdr->cdr->car= inc(q);
	m1= evaluate(inc(t), IMPLODE);
	dec(m1);

	/* Compute q - p. */
	t->car= q;
	t->cdr->cdr->car= p;
	m2= evaluate(t, IMPLODE);
	dec(m2);

	/* Both results must be empty. */
	return m1 == nil && m2 == nil;
}

int wordlist(cell_t **pw, int atom)
/* Check if p is a list of words, typically an imploded list.  Return
 * the number of words seen, -1 if they are not words (INPUT/OUTPUT?).
 * If atom is true than a list of one word is turned into a word.
 */
{
	int n= 0;
	cell_t *p, **pp= pw;

	while (*pp != nil) {
		*pp= append(CELL, *pp);
		p= (*pp)->car;
		n= n >= 0 && p != nil && p->type == WORD ? n+1 : -1;
		pp= &(*pp)->cdr;
	}
	if (atom && n == 1) *pw= go(*pw, (*pw)->car);
	return n;
}

char *template;		/* Current name of a temporary file. */
static char *tp;	/* Current place withing the tempfile. */

char *maketemp(void)
/* Return a name that can be used as a temporary filename. */
{
	int i= 0;

	if (tp == nil) {
		size_t len= strlen(template);

		template= allocate(template, (len+20) * sizeof(*template));
		sprintf(template+len, "/acd%d", getpid());
		tp= template + strlen(template);
	}

	for (;;) {
		switch (tp[i]) {
		case 0:		tp[i]= 'a';
				tp[i+1]= 0;	return template;
		case 'z':	tp[i++]= 'a';	break;
		default:	tp[i]++;	return template;
		}
	}
}

void inittemp(char *tmpdir)
/* Initialize the temporary filename generator. */
{
	template= allocate(nil, (strlen(tmpdir)+20) * sizeof(*template));
	sprintf(template, "%s/acd%d", tmpdir, getpid());
	tp= template + strlen(template);

	/* Create a directory within tempdir that we can safely play in. */
	while (action != 1 && mkdir(template, 0700) < 0) {
		if (errno == EEXIST) {
			(void) maketemp();
		} else {
			report(template);
			action= 0;
		}
	}
	if (verbose >= 2) printf("mkdir %s\n", template);
	while (*tp != 0) tp++;
	*tp++= '/';
	*tp= 0;
}

void deltemp(void)
/* Remove our temporary temporaries directory. */
{
	while (*--tp != '/') {}
	*tp = 0;
	if (rmdir(template) < 0 && errno != ENOENT) report(template);
	if (verbose >= 2) printf("rmdir %s\n", template);
	deallocate(template);
}

cell_t *splitenv(char *env)
/* Split a string from the environment into several words at whitespace
 * and colons.  Two colons (::) become a dot.
 */
{
	cell_t *r= nil, **pr= &r;
	char *p;

	do {
		while (*env != 0 && isspace(*env)) env++;

		if (*env == 0) break;

		p= env;
		while (*p != 0 && !isspace(*p) && *p != ':') p++;

		*pr= cons(CELL,
			p == env ? findword(".") : findnword(env, p-env));
		pr= &(*pr)->cdr;
		env= p;
	} while (*env++ != 0);
	return r;
}

void key_usage(char *how)
{
	fprintf(stderr, "\"%s\", line %u: Usage: %s %s\n",
		descr, pc->lineno, pc->line->car->name, how);
	action= 0;
}

void inappropriate(void)
{
	fprintf(stderr, "\"%s\", line %u: wrong execution phase for '%s'\n",
		descr, pc->lineno, pc->line->car->name);
	action= 0;
}

int readonly(cell_t *v)
{
	if (v->flags & W_RDONLY) {
		fprintf(stderr, "\"%s\", line %u: %s is read-only\n",
			descr, pc->lineno, v->name);
		action= 0;
		return 1;
	}
	return 0;
}

void complain(cell_t *err)
/* acd: err ... */
{
	cell_t *w;

	fprintf(stderr, "%s:", program);

	while (err != nil) {
		if (err->type == CELL) {
			w= err->car; err= err->cdr;
		} else {
			w= err; err= nil;
		}
		fprintf(stderr, " %s", w->name);
	}
	action= 0;
}

int keyword(char *name)
/* True if the current line is headed by the given keyword. */
{
	cell_t *t;

	return (t= pc->line) != nil && t->type == CELL
		&& (t= t->car) != nil && t->type == WORD
		&& strcmp(t->name, name) == 0;
}

cell_t *getvar(cell_t *v)
/* Return a word or the word referenced by a subst. */
{
	if (v == nil) return nil;
	if (v->type == WORD) return v;
	if (v->type == SUBST) return v->subst;
	return nil;
}

void argscan(void), compile(void);
void transform(rule_t *);

void exec_one(void)
/* Execute one line of the program. */
{
	cell_t *v, *p, *q, *r, *t;
	unsigned n= 0;
	static int last_if= 1;

	/* Description file this line came from. */
	descr= pc->file->name;

	for (p= pc->line; p != nil; p= p->cdr) n++;

	if (n == 0) return;	/* Null statement. */

	p= pc->line;
	q= p->cdr;
	r= q == nil ? nil : q->cdr;

	/* Try one by one all the different commands. */

	if (n >= 2 && q->car != nil && q->car->type == EQUALS) {
		/* An assignment. */
		int flags;

		if ((v= getvar(p->car)) == nil) {
			fprintf(stderr,
				"\"%s\", line %u: Usage: <var> = expr ...\n",
				descr, pc->lineno);
			action= 0;
			return;
		}

		if (readonly(v)) return;

		flags= v->flags;
		v->flags|= W_LOCAL|W_RDONLY;
		t= evaluate(inc(r), PARTIAL);
		dec(v->value);
		v->value= t;
		v->flags= flags | W_SET;
		if (talk()) {
			printf("%s =\b=\b= ", v->name);
			prin2n(t);
		}
	} else
	if (keyword("unset")) {
		/* Set a variable to "undefined". */

		if (n != 2 || (v= getvar(q->car)) == nil) {
			key_usage("<var>");
			return;
		}
		if (readonly(v)) return;

		if (talk()) prin2n(p);

		dec(v->value);
		v->value= nil;
		v->flags&= ~W_SET;
	} else
	if (keyword("import")) {
		/* Import a variable from the UNIX environment. */
		char *env;

		if (n != 2 || (v= getvar(q->car)) == nil) {
			key_usage("<var>");
			return;
		}
		if (readonly(v)) return;

		if ((env= getenv(v->name)) == nil) return;

		if (talk()) printf("import %s=%s\n", v->name, env);

		t= splitenv(env);
		dec(v->value);
		v->value= t;
		v->flags|= W_SET;
	} else
	if (keyword("mktemp")) {
		/* Assign a variable the name of a temporary file. */
		char *tmp, *suff;

		r= evaluate(inc(r), IMPLODE);
		if (n == 3 && wordlist(&r, 1) != 1) n= 0;

		if ((n != 2 && n != 3) || (v= getvar(q->car)) == nil) {
			dec(r);
			key_usage("<var> [<suffix>]");
			return;
		}
		if (readonly(v)) { dec(r); return; }

		tmp= maketemp();
		suff= r == nil ? "" : r->name;

		t= newcell();
		t->type= WORD;
		t->name= allocate(nil,
			(strlen(tmp) + strlen(suff) + 1) * sizeof(*t->name));
		strcpy(t->name, tmp);
		strcat(t->name, suff);
		t= inc(t);
		dec(r);
		dec(v->value);
		v->value= t;
		v->flags|= W_SET;
		t->flags|= W_TEMP;
		if (talk()) printf("mktemp %s=%s\n", v->name, t->name);
	} else
	if (keyword("temporary")) {
		/* Mark a word as a temporary file. */
		cell_t *tmp;

		tmp= evaluate(inc(q), IMPLODE);

		if (wordlist(&tmp, 1) < 0) {
			dec(tmp);
			key_usage("<word>");
			return;
		}
		if (talk()) printf("temporary %s\n", tmp->name);

		tmp->flags|= W_TEMP;
		dec(tmp);
	} else
	if (keyword("stop")) {
		/* Set the suffix to stop the transformation on. */
		cell_t *suff;

		if (phase > SCAN) { inappropriate(); return; }

		suff= evaluate(inc(q), IMPLODE);

		if (wordlist(&suff, 1) != 1) {
			dec(suff);
			key_usage("<suffix>");
			return;
		}
		dec(V_stop);
		V_stop= suff;
		if (talk()) printf("stop %s\n", suff->name);
	} else
	if (keyword("numeric")) {
		/* Check if a string denotes a number, like $n in -O$n. */
		cell_t *num;
		char *pn;

		num= evaluate(inc(q), IMPLODE);

		if (wordlist(&num, 1) != 1) {
			dec(num);
			key_usage("<arg>");
			return;
		}
		if (talk()) printf("numeric %s\n", num->name);

		(void) strtoul(num->name, &pn, 10);
		if (*pn != 0) {
			complain(phase == SCAN ? V_star->value : nil);
			if (phase == SCAN) fputc(':', stderr);
			fprintf(stderr, " '%s' is not a number\n", num->name);
		}
		dec(num);
	} else
	if (keyword("error")) {
		/* Signal an error. */
		cell_t *err;

		err= evaluate(inc(q), IMPLODE);

		if (wordlist(&err, 0) < 1) {
			dec(err);
			key_usage("expr ...");
			return;
		}

		if (talk()) { printf("error "); prin2n(err); }

		complain(err);
		fputc('\n', stderr);
		dec(err);
	} else
	if (keyword("if")) {
		/* if (list) = (list) using set comparison. */
		int eq;

		if (n != 4 || r->car == nil || r->car->type != EQUALS) {
			key_usage("<expr> = <expr>");
			execute(DONT, pc->indent+1);
			last_if= 1;
			return;
		}
		q= q->car;
		r= r->cdr->car;
		if (talk()) {
			printf("if ");
			prin1(t= evaluate(inc(q), IMPLODE));
			dec(t);
			printf(" = ");
			prin1n(t= evaluate(inc(r), IMPLODE));
			dec(t);
		}
		eq= equal(q, r);
		execute(eq ? DOIT : DONT, pc->indent+1);
		last_if= eq;
	} else
	if (keyword("ifdef") || keyword("ifndef")) {
		/* Is a variable defined or undefined? */
		int doit;

		if (n != 2 || (v= getvar(q->car)) == nil) {
			key_usage("<var>");
			execute(DONT, pc->indent+1);
			last_if= 1;
			return;
		}
		if (talk()) prin2n(p);

		doit= ((v->flags & W_SET) != 0) ^ (p->car->name[2] == 'n');
		execute(doit ? DOIT : DONT, pc->indent+1);
		last_if= doit;
	} else
	if (keyword("iftemp") || keyword("ifhash")) {
		/* Is a file a temporary file? */
		/* Does a file need preprocessing? */
		cell_t *file;
		int doit= 0;

		file= evaluate(inc(q), IMPLODE);

		if (wordlist(&file, 1) != 1) {
			dec(file);
			key_usage("<arg>");
			return;
		}
		if (talk()) printf("%s %s\n", p->car->name, file->name);

		if (p->car->name[2] == 't') {
			/* iftemp file */
			if (file->flags & W_TEMP) doit= 1;
		} else {
			/* ifhash file */
			int fd;
			char hash;

			if ((fd= open(file->name, O_RDONLY)) >= 0) {
				if (read(fd, &hash, 1) == 1 && hash == '#')
					doit= 1;
				close(fd);
			}
		}
		dec(file);

		execute(doit ? DOIT : DONT, pc->indent+1);
		last_if= doit;
	} else
	if (keyword("else")) {
		/* Else clause for an if, ifdef, or ifndef. */
		if (n != 1) {
			key_usage("");
			execute(DONT, pc->indent+1);
			return;
		}
		if (talk()) prin2n(p);

		execute(!last_if ? DOIT : DONT, pc->indent+1);
	} else
	if (keyword("treat")) {
		/* Treat a file as having a certain suffix. */

		if (phase > SCAN) { inappropriate(); return; }

		if (n == 3) {
			q= evaluate(inc(q->car), IMPLODE);
			r= evaluate(inc(r->car), IMPLODE);
		}
		if (n != 3 || wordlist(&q, 1) != 1 || wordlist(&r, 1) != 1) {
			if (n == 3) { dec(q); dec(r); }
			key_usage("<file> <suffix>");
			return;
		}
		if (talk()) printf("treat %s %s\n", q->name, r->name);

		dec(q->suffix);
		q->suffix= r;
		q->flags|= W_SUFF;
		dec(q);
	} else
	if (keyword("apply")) {
		/* Apply a transformation rule to the current input file. */
		rule_t *rule, *sav_path;
		cell_t *sav_wait, *sav_in, *sav_out;
		program_t *sav_next;

		if (phase != COMPILE) { inappropriate(); return; }

		if (V_star->value->cdr != nil) {
			fprintf(stderr, "\"%s\", line %u: $* is not one file\n",
				descr, pc->lineno);
			action= 0;
			return;
		}
		if (n == 3) {
			q= evaluate(inc(q->car), IMPLODE);
			r= evaluate(inc(r->car), IMPLODE);
		}
		if (n != 3 || wordlist(&q, 1) != 1 || wordlist(&r, 1) != 1) {
			if (n == 3) { dec(q); dec(r); }
			key_usage("<file> <suffix>");
			return;
		}
		if (talk()) printf("apply %s %s\n", q->name, r->name);

		/* Find a rule */
		for (rule= rules; rule != nil; rule= rule->next) {
			if (rule->type == TRANSFORM
				&& rule->from == q && rule->to == r) break;
		}
		if (rule == nil) {
			fprintf(stderr,
				"\"%s\", line %u: no %s %s transformation\n",
				descr, pc->lineno, q->name, r->name);
			action= 0;
		}
		dec(q);
		dec(r);
		if (rule == nil) return;

		/* Save the world. */
		sav_path= rule->path;
		sav_wait= rule->wait;
		sav_in= V_in->value;
		sav_out= V_out->value;
		sav_next= nextpc;

		/* Isolate the rule and give it new input. */
		rule->path= rule;
		rule->wait= V_star->value;
		V_star->value= nil;
		V_in->value= nil;
		V_out->value= nil;

		transform(rule);

		/* Retrieve the new $* and repair. */
		V_star->value= rule->wait;
		rule->path= sav_path;
		rule->wait= sav_wait;
		V_in->value= sav_in;
		V_out->value= sav_out;
		V_out->flags= W_SET|W_LOCAL;
		nextpc= sav_next;
	} else
	if (keyword("include")) {
		/* Include another description file into this program. */
		cell_t *file;
		program_t *incl, *prog, **ppg= &prog;

		file= evaluate(inc(q), IMPLODE);

		if (wordlist(&file, 1) != 1) {
			dec(file);
			key_usage("<file>");
			return;
		}
		if (talk()) printf("include %s\n", file->name);
		descr= file->name;
		incl= pc;
		prog= get_prog();
		dec(file);

		/* Raise the program to the include's indent level. */
		while (*ppg != nil) {
			(*ppg)->indent += incl->indent;
			ppg= &(*ppg)->next;
		}

		/* Kill the include and splice the included program in. */
		dec(incl->line);
		incl->line= nil;
		*ppg= incl->next;
		incl->next= prog;
		pc= incl;
		nextpc= prog;
	} else
	if (keyword("arg")) {
		/* An argument scanning rule. */

		if (phase > SCAN) { inappropriate(); return; }

		if (n < 2) {
			key_usage("<string> ...");
			execute(DONT, pc->indent+1);
			return;
		}
		if (talk()) prin2n(p);

		newrule(ARG, inc(q), nil);

		/* Always skip the body, it comes later. */
		execute(DONT, pc->indent+1);
	} else
	if (keyword("transform")) {
		/* A file transformation rule. */

		if (phase > SCAN) { inappropriate(); return; }

		if (n == 3) {
			q= evaluate(inc(q->car), IMPLODE);
			r= evaluate(inc(r->car), IMPLODE);
		}
		if (n != 3 || wordlist(&q, 1) != 1 || wordlist(&r, 1) != 1) {
			if (n == 3) { dec(q); dec(r); }
			key_usage("<suffix1> <suffix2>");
			execute(DONT, pc->indent+1);
			return;
		}
		if (talk()) printf("transform %s %s\n", q->name, r->name);

		newrule(TRANSFORM, q, r);

		/* Body comes later. */
		execute(DONT, pc->indent+1);
	} else
	if (keyword("prefer")) {
		/* Prefer a transformation over others. */

		if (phase > SCAN) { inappropriate(); return; }

		if (n == 3) {
			q= evaluate(inc(q->car), IMPLODE);
			r= evaluate(inc(r->car), IMPLODE);
		}
		if (n != 3 || wordlist(&q, 1) != 1 || wordlist(&r, 1) != 1) {
			if (n == 3) { dec(q); dec(r); }
			key_usage("<suffix1> <suffix2>");
			return;
		}
		if (talk()) printf("prefer %s %s\n", q->name, r->name);

		newrule(PREFER, q, r);
	} else
	if (keyword("combine")) {
		/* A file combination (loader) rule. */

		if (phase > SCAN) { inappropriate(); return; }

		if (n == 3) {
			q= evaluate(inc(q->car), IMPLODE);
			r= evaluate(inc(r->car), IMPLODE);
		}
		if (n != 3 || wordlist(&q, 0) < 1 || wordlist(&r, 1) != 1) {
			if (n == 3) { dec(q); dec(r); }
			key_usage("<suffix-list> <suffix>");
			execute(DONT, pc->indent+1);
			return;
		}
		if (talk()) {
			printf("combine ");
			prin1(q);
			printf(" %s\n", r->name);
		}

		newrule(COMBINE, q, r);

		/* Body comes later. */
		execute(DONT, pc->indent+1);
	} else
	if (keyword("scan") || keyword("compile")) {
		program_t *next= nextpc;

		if (n != 1) { key_usage(""); return; }
		if (phase != INIT) { inappropriate(); return; }

		if (talk()) prin2n(p);

		argscan();
		if (p->car->name[0] == 'c') compile();
		nextpc= next;
	} else {
		/* A UNIX command. */
		t= evaluate(inc(pc->line), IMPLODE);
		unix_exec(t);
		dec(t);
	}
}

void execute(exec_t how, unsigned indent)
/* Execute (or skip) all lines with at least the given indent. */
{
	int work= 0;	/* Need to execute at least one line. */
	unsigned firstline;
	unsigned nice_indent= 0;	/* 0 = Don't know what's nice yet. */

	if (pc == nil) return;	/* End of program. */

	firstline= pc->lineno;

	if (how == DONT) {
		/* Skipping a body, but is there another guard? */
		pc= pc->next;
		if (pc != nil && pc->indent < indent && pc->line != nil) {
			/* There is one!  Bail out, then it get's executed. */
			return;
		}
	} else {
		/* Skip lines with a lesser indentation, they are guards for
		 * the same substatements.  Don't go past empty lines.
		 */
		while (pc != nil && pc->indent < indent && pc->line != nil)
			pc= pc->next;
	}

	/* Execute all lines with an indentation of at least "indent". */
	while (pc != nil && pc->indent >= indent) {
		if (pc->indent != nice_indent && how == DOIT) {
			if (nice_indent != 0) {
				fprintf(stderr,
			"\"%s\", line %u: (warning) sudden indentation shift\n",
					descr, pc->lineno);
			}
			nice_indent= pc->indent;
		}
		nextpc= pc->next;
		if (how == DOIT) exec_one();
		pc= nextpc;
		work= 1;
	}

	if (indent > 0 && !work) {
		fprintf(stderr, "\"%s\", line %u: empty body, no statements\n",
			descr, firstline);
		action= 0;
	}
}

int argmatch(int shift, cell_t *match, cell_t *match1, char *arg1)
/* Try to match an arg rule to the input file list L_args.  Execute the arg
 * body (pc is set to it) on success.
 */
{
	cell_t *oldval, *v;
	int m, oldflags;
	size_t i, len;
	int minus= 0;

	if (shift) {
		/* An argument has been accepted and may be shifted to $*. */
		cell_t **oldpstar= pV_star;
		*pV_star= L_args;
		L_args= *(pV_star= &L_args->cdr);
		*pV_star= nil;

		if (argmatch(0, match->cdr, nil, nil)) return 1;

		/* Undo the damage. */
		*pV_star= L_args;
		L_args= *(pV_star= oldpstar);
		*pV_star= nil;
		return 0;
	}

	if (match == nil) {
		/* A full match, execute the arg body. */

		/* Enable $>. */
		V_out->flags= W_SET|W_LOCAL;

		if (verbose >= 3) {
			prin2(pc->line);
			printf(" =\b=\b= ");
			prin2n(V_star->value);
		}
		execute(DOIT, pc->indent+1);

		/* Append $> to the file list. */
		if (V_out->value != nil) {
			*pL_files= cons(CELL, V_out->value);
			pL_files= &(*pL_files)->cdr;
		}

		/* Disable $>. */
		V_out->value= nil;
		V_out->flags= W_SET|W_LOCAL|W_RDONLY;

		return 1;
	}

	if (L_args == nil) return 0;	/* Out of arguments to match. */

	/* Match is a list of words, substs and strings containing letters and
	 * substs.  Match1 is the current element of the first element of match.
	 * Arg1 is the current character of the first element of L_args.
	 */
	if (match1 == nil) {
		/* match1 is at the end of a string, then arg1 must also. */
		if (arg1 != nil) {
			if (*arg1 != 0) return 0;
			return argmatch(1, match, nil, nil);
		}
		/* If both are nil: Initialize. */
		match1= match->car;
		arg1= L_args->car->name;

		/* A subst may not match a leading '-'. */
		if (arg1[0] == '-') minus= 1;
	}

	if (match1->type == WORD && strcmp(match1->name, arg1) == 0) {
		/* A simple match of an argument. */

		return argmatch(1, match, nil, nil);
	}

	if (match1->type == SUBST && !minus) {
		/* A simple match of a subst. */

		/* The variable gets the first of the arguments as its value. */
		v= match1->subst;
		if (v->flags & W_RDONLY) return 0;	/* ouch */
		oldflags= v->flags;
		v->flags= W_SET|W_LOCAL|W_RDONLY;
		oldval= v->value;
		v->value= inc(L_args->car);

		m= argmatch(1, match, nil, nil);

		/* Recover the value of the variable. */
		dec(v->value);
		v->flags= oldflags;
		v->value= oldval;
		return m;
	}
	if (match1->type != STRING) return 0;

	/* Match the first item in the string. */
	if (match1->car == nil) return 0;

	if (match1->car->type == LETTER
			&& match1->car->letter == (unsigned char) *arg1) {
		/* A letter matches, try the rest of the string. */

		return argmatch(0, match, match1->cdr, arg1+1);
	}

	/* It can only be a subst in a string now. */
	len= strlen(arg1);
	if (match1->car->type != SUBST || minus || len == 0) return 0;

	/* The variable can match from 1 character to all of the argument.
	 * Matching as few characters as possible happens to be the Right Thing.
	 */
	v= match1->car->subst;
	if (v->flags & W_RDONLY) return 0;	/* ouch */
	oldflags= v->flags;
	v->flags= W_SET|W_LOCAL|W_RDONLY;
	oldval= v->value;

	m= 0;
	for (i= match1->cdr == nil ? len : 1; !m && i <= len; i++) {
		v->value= findnword(arg1, i);

		m= argmatch(0, match, match1->cdr, arg1+i);

		dec(v->value);
	}
	/* Recover the value of the variable. */
	v->flags= oldflags;
	v->value= oldval;
	return m;
}

void argscan(void)
/* Match all the arguments to the arg rules, those that don't match are
 * used as files for transformation.
 */
{
	rule_t *rule;
	int m;

	phase= SCAN;

	/* Process all the arguments. */
	while (L_args != nil) {
		pV_star= &V_star->value;

		/* Try all the arg rules. */
		m= 0;
		for (rule= rules; !m && rule != nil; rule= rule->next) {
			if (rule->type != ARG) continue;

			pc= rule->prog;

			m= argmatch(0, rule->match, nil, nil);
		}
		dec(V_star->value);
		V_star->value= nil;

		/* On failure, add the first argument to the list of files. */
		if (!m) {
			*pL_files= L_args;
			L_args= *(pL_files= &L_args->cdr);
			*pL_files= nil;
		}
	}
	phase= INIT;
}

int member(cell_t *p, cell_t *l)
/* True if p is a member of list l. */
{
	while (l != nil && l->type == CELL) {
		if (p == l->car) return 1;
		l= l->cdr;
	}
	return p == l;
}

long basefind(cell_t *f, cell_t *l)
/* See if f has a suffix in list l + set the base name of f.
 * -1 if not found, preference number for a short basename otherwise. */
{
	cell_t *suff;
	size_t blen, slen;
	char *base;

	/* Determine base name of f, with suffix. */
	if ((base= strrchr(f->name, '/')) == nil) base= f->name; else base++;
	blen= strlen(base);

	/* Try suffixes. */
	while (l != nil) {
		if (l->type == CELL) {
			suff= l->car; l= l->cdr;
		} else {
			suff= l; l= nil;
		}
		if (f->flags & W_SUFF) {
			/* F has a suffix imposed on it. */
			if (f->suffix == suff) return 0;
			continue;
		}
		slen= strlen(suff->name);
		if (slen < blen && strcmp(base+blen-slen, suff->name) == 0) {
			/* Got it! */
			dec(f->base);
			f->base= findnword(base, blen-slen);
			return 10000L * (blen - slen);
		}
	}
	return -1;
}

#define NO_PATH		2000000000	/* No path found yet. */

long shortest;		/* Length of the shortest path as yet. */

rule_t *findpath(long depth, int seek, cell_t *file, rule_t *start)
/* Find the path of the shortest transformation to the stop suffix. */
{
	rule_t *rule;

	if (action == 0) return nil;

	if (start == nil) {
		/* No starting point defined, find one using "file". */

		for (rule= rules; rule != nil; rule= rule->next) {
			if (rule->type < TRANSFORM) continue;

			if ((depth= basefind(file, rule->from)) >= 0) {
				if (findpath(depth, seek, nil, rule) != nil)
					return rule;
			}
		}
		return nil;
	}

	/* Cycle? */
	if (start->path != nil) {
		/* We can't have cycles through combines. */
		if (start->type == COMBINE) {
			fprintf(stderr,
				"\"%s\": contains a combine-combine cycle\n",
				descr);
			action= 0;
		}
		return nil;
	}

	/* Preferred transformations are cheap. */
	if (start->flags & R_PREFER) depth-= 100;

	/* Try to go from start closer to the stop suffix. */
	for (rule= rules; rule != nil; rule= rule->next) {
		if (rule->type < TRANSFORM) continue;

		if (member(start->to, rule->from)) {
			start->path= rule;
			rule->npaths++;
			if (findpath(depth+1, seek, nil, rule) != nil)
				return start;
			start->path= nil;
			rule->npaths--;
		}
	}

	if (V_stop == nil) {
		fprintf(stderr, "\"%s\": no stop suffix has been defined\n",
			descr);
		action= 0;
		return nil;
	}

	/* End of the line? */
	if (start->to == V_stop) {
		/* Got it. */
		if (seek) {
			/* Second hunt, do we find the shortest? */
			if (depth == shortest) return start;
		} else {
			/* Is this path shorter than the last one? */
			if (depth < shortest) shortest= depth;
		}
	}
	return nil;	/* Fail. */
}

void transform(rule_t *rule)
/* Transform the file(s) connected to the rule according to the rule. */
{
	cell_t *file, *in, *out;
	char *base;

	/* Let $* be the list of input files. */
	while (rule->wait != nil) {
		file= rule->wait;
		rule->wait= file->cdr;
		file->cdr= V_star->value;
		V_star->value= file;
	}

	/* Set $< to the basename of the first input file. */
	file= file->car;
	V_in->value= in= inc(file->flags & W_SUFF ? file : file->base);
	file->flags&= ~W_SUFF;

	/* Set $> to the output file name of the transformation. */
	out= newcell();
	out->type= WORD;
	base= rule->path == nil ? in->name : maketemp();
	out->name= allocate(nil,
		(strlen(base)+strlen(rule->to->name)+1) * sizeof(*out->name));
	strcpy(out->name, base);
	if (rule->path == nil || strchr(rule->to->name, '/') == nil)
		strcat(out->name, rule->to->name);
	out= inc(out);
	if (rule->path != nil) out->flags|= W_TEMP;

	V_out->value= out;
	V_out->flags= W_SET|W_LOCAL;

	/* Do a transformation.  (Finally) */
	if (verbose >= 3) {
		printf("%s ", rule->type==TRANSFORM ? "transform" : "combine");
		prin2(V_star->value);
		printf(" %s\n", out->name);
	}
	pc= rule->prog;
	execute(DOIT, pc->indent+1);

	/* Hand $> over to the next rule, it must be a single word. */
	out= evaluate(V_out->value, IMPLODE);
	if (wordlist(&out, 1) != 1) {
		fprintf(stderr,
		"\"%s\", line %u: $> should be returned as a single word\n",
			descr, rule->prog->lineno);
		action= 0;
	}

	if ((rule= rule->path) != nil) {
		/* There is a next rule. */
		dec(out->base);
		out->base= in;		/* Basename of input file. */
		file= inc(newcell());
		file->car= out;
		file->cdr= rule->wait;
		rule->wait= file;
	} else {
		dec(in);
		dec(out);
	}

	/* Undo the damage to $*, $<, and $>. */
	dec(V_star->value);
	V_star->value= nil;
	V_in->value= nil;
	V_out->value= nil;
	V_out->flags= W_SET|W_LOCAL|W_RDONLY;
}

void compile(void)
{
	rule_t *rule;
	cell_t *file, *t;

	phase= COMPILE;

	/* Implode the files list. */
	L_files= evaluate(L_files, IMPLODE);
	if (wordlist(&L_files, 0) < 0) {
		fprintf(stderr, "\"%s\": An assignment to $> contained junk\n",
			descr);
		action= 0;
	}

	while (action != 0 && L_files != nil) {
		file= L_files->car;

		/* Initialize. */
		shortest= NO_PATH;
		for (rule= rules; rule != nil; rule= rule->next)
			rule->path= nil;

		/* Try all possible transformation paths. */
		(void) findpath(0L, 0, file, nil);

		if (shortest == NO_PATH) {	/* Can't match the file. */
			fprintf(stderr,
			"%s: %s: can't compile, no transformation applies\n",
				program, file->name);
			action= 0;
			return;
		}

		/* Find the first short path. */
		if ((rule= findpath(0L, 1, file, nil)) == nil) return;

		/* Transform the file until you hit a combine. */
		t= inc(newcell());
		t->car= inc(file);
		L_files= go(L_files, L_files->cdr);
		t->cdr= rule->wait;
		rule->wait= t;
		while (action != 0 && rule != nil && rule->type != COMBINE) {
			transform(rule);
			rule= rule->path;
		}
	}

	/* All input files have been transformed to combine rule(s).  Now
	 * we need to find the combine rule with the least number of paths
	 * running through it (this combine may be followed by another) and
	 * transform from there.
	 */
	while (action != 0) {
		int least;
		rule_t *comb= nil;

		for (rule= rules; rule != nil; rule= rule->next) {
			rule->path= nil;

			if (rule->type != COMBINE || rule->wait == nil)
				continue;

			if (comb == nil || rule->npaths < least) {
				least= rule->npaths;
				comb= rule;
			}
		}

		/* No combine?  Then we're done. */
		if (comb == nil) break;

		/* Initialize. */
		shortest= NO_PATH;

		/* Try all possible transformation paths. */
		(void) findpath(0L, 0, nil, comb);

		if (shortest == NO_PATH) break;

		/* Find the first short path. */
		if ((rule= findpath(0L, 1, nil, comb)) == nil) return;

		/* Transform until you hit another combine. */
		do {
			transform(rule);
			rule= rule->path;
		} while (action != 0 && rule != nil && rule->type != COMBINE);
	}
	phase= INIT;
}

cell_t *predef(char *var, char *val)
/* A predefined variable var with value val, or a special variable. */
{
	cell_t *p, *t;

	p= findword(var);
	if (val != nil) {	/* Predefined. */
		t= findword(val);
		dec(p->value);
		p->value= t;
		p->flags|= W_SET;
		if (verbose >= 3) {
			prin1(p);
			printf(" =\b=\b= ");
			prin2n(t);
		}
	} else {		/* Special: $* and such. */
		p->flags= W_SET|W_LOCAL|W_RDONLY;
	}
	t= inc(newcell());
	t->car= p;
	t->cdr= L_predef;
	L_predef= t;
	return p;
}

void usage(void)
{
	fprintf(stderr,
	"Usage: %s -v<n> -vn<n> -name <name> -descr <descr> -T <dir> ...\n",
		program);
	exit(-1);
}

int main(int argc, char **argv)
{
	char *tmpdir;
	program_t *prog;
	cell_t **pa;
	int i;

	/* Call name of the program, decides which description to use. */
	if ((program= strrchr(argv[0], '/')) == nil)
		program= argv[0];
	else
		program++;

	/* Directory for temporary files. */
	if ((tmpdir= getenv("TMPDIR")) == nil || *tmpdir == 0)
		tmpdir= "/tmp";

	/* Transform arguments to a list, processing the few ACD options. */
	pa= &L_args;
	for (i= 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'v') {
			char *a= argv[i]+2;

			if (*a == 'n') { a++; action= 1; }
			verbose= 2;

			if (*a != 0) {
				verbose= strtoul(a, &a, 10);
				if (*a != 0) usage();
			}
		} else
		if (strcmp(argv[i], "-name") == 0) {
			if (++i == argc) usage();
			program= argv[i];
		} else
		if (strcmp(argv[i], "-descr") == 0) {
			if (++i == argc) usage();
			descr= argv[i];
		} else
		if (argv[i][0] == '-' && argv[i][1] == 'T') {
			if (argv[i][2] == 0) {
				if (++i == argc) usage();
				tmpdir= argv[i];
			} else
				tmpdir= argv[i]+2;
		} else {
			/* Any other argument must be processed. */
			*pa= cons(CELL, findword(argv[i]));
			pa= &(*pa)->cdr;
		}
	}
#ifndef DESCR
	/* Default description file is based on the program name. */
	if (descr == nil) descr= program;
#else
	/* Default description file is predefined. */
	if (descr == nil) descr= DESCR;
#endif

	inittemp(tmpdir);

	/* Catch user signals. */
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN) signal(SIGHUP, interrupt);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN) signal(SIGINT, interrupt);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN) signal(SIGTERM, interrupt);

	/* Predefined or special variables. */
	predef("PROGRAM", program);
	predef("VERSION", version);
#ifdef ARCH
	predef("ARCH", ARCH);		/* Cross-compilers like this. */
#endif
	V_star= predef("*", nil);
	V_in= predef("<", nil);
	V_out= predef(">", nil);

	/* Read the description file. */
	if (verbose >= 3) printf("include %s\n", descr);
	prog= get_prog();

	phase= INIT;
	pc= prog;
	execute(DOIT, 0);

	argscan();
	compile();

	/* Delete all allocated data to test inc/dec balance. */
	while (prog != nil) {
		program_t *junk= prog;
		prog= junk->next;
		dec(junk->file);
		dec(junk->line);
		deallocate(junk);
	}
	while (rules != nil) {
		rule_t *junk= rules;
		rules= junk->next;
		dec(junk->from);
		dec(junk->to);
		dec(junk->wait);
		deallocate(junk);
	}
	deltemp();
	dec(V_stop);
	dec(L_args);
	dec(L_files);
	dec(L_predef);

	quit(action == 0 ? 1 : 0);
}
