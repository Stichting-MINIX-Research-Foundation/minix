/*	tokenize.c - split input into tokens		Author: Kees J. Bot
 *								13 Dec 1993
 */
#define nil 0
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "asmconv.h"
#include "token.h"

static FILE *tf;
static char *tfile;
static char *orig_tfile;
static int tcomment;
static int tc;
static long tline;
static token_t *tq;

static void readtc(void)
/* Read one character from the input file and put it in the global 'tc'. */
{
	static int nl= 0;

	if (nl) tline++;
	if ((tc= getc(tf)) == EOF && ferror(tf)) fatal(orig_tfile);
	nl= (tc == '\n');
}

void set_file(char *file, long line)
/* Set file name and line number, changed by a preprocessor trick. */
{
	deallocate(tfile);
	tfile= allocate(nil, (strlen(file) + 1) * sizeof(tfile[0]));
	strcpy(tfile, file);
	tline= line;
}

void get_file(char **file, long *line)
/* Get file name and line number. */
{
	*file= tfile;
	*line= tline;
}

void parse_err(int err, token_t *t, const char *fmt, ...)
/* Report a parsing error. */
{
	va_list ap;

	fprintf(stderr, "\"%s\", line %ld: ", tfile,
						t == nil ? tline : t->line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (err) set_error();
}

void tok_init(char *file, int comment)
/* Open the file to tokenize and initialize the tokenizer. */
{
	if (file == nil) {
		file= "stdin";
		tf= stdin;
	} else {
		if ((tf= fopen(file, "r")) == nil) fatal(file);
	}
	orig_tfile= file;
	set_file(file, 1);
	readtc();
	tcomment= comment;
}

static int isspace(int c)
{
	return between('\0', c, ' ') && c != '\n';
}

#define iscomment(c)	((c) == tcomment)

static int isidentchar(int c)
{
	return between('a', c, 'z')
		|| between('A', c, 'Z')
		|| between('0', c, '9')
		|| c == '.'
		|| c == '_'
		;
}

static token_t *new_token(void)
{
	token_t *new;

	new= allocate(nil, sizeof(*new));
	new->next= nil;
	new->line= tline;
	new->name= nil;
	new->symbol= -1;
	return new;
}

static token_t *get_word(void)
/* Read one word, an identifier, a number, a label, or a mnemonic. */
{
	token_t *w;
	char *name;
	size_t i, len;

	i= 0;
	len= 16;
	name= allocate(nil, len * sizeof(name[0]));

	while (isidentchar(tc)) {
		name[i++]= tc;
		readtc();
		if (i == len) name= allocate(name, (len*= 2) * sizeof(name[0]));
	}
	name[i]= 0;
	name= allocate(name, (i+1) * sizeof(name[0]));
	w= new_token();
	w->type= T_WORD;
	w->name= name;
	w->len= i;
	return w;
}

static token_t *get_string(void)
/* Read a single or double quotes delimited string. */
{
	token_t *s;
	int quote;
	char *str;
	size_t i, len;
	int n, j;
	int seen;

	quote= tc;
	readtc();

	i= 0;
	len= 16;
	str= allocate(nil, len * sizeof(str[0]));

	while (tc != quote && tc != '\n' && tc != EOF) {
		seen= -1;
		if (tc == '\\') {
			readtc();
			if (tc == '\n' || tc == EOF) break;

			switch (tc) {
			case 'a':	tc= '\a'; break;
			case 'b':	tc= '\b'; break;
			case 'f':	tc= '\f'; break;
			case 'n':	tc= '\n'; break;
			case 'r':	tc= '\r'; break;
			case 't':	tc= '\t'; break;
			case 'v':	tc= '\v'; break;
			case 'x':
				n= 0;
				for (j= 0; j < 3; j++) {
					readtc();
					if (between('0', tc, '9'))
						tc-= '0' + 0x0;
					else
					if (between('A', tc, 'A'))
						tc-= 'A' + 0xA;
					else
					if (between('a', tc, 'a'))
						tc-= 'a' + 0xa;
					else {
						seen= tc;
						break;
					}
					n= n*0x10 + tc;
				}
				tc= n;
				break;
			default:
				if (!between('0', tc, '9')) break;
				n= 0;
				for (j= 0; j < 3; j++) {
					if (between('0', tc, '9'))
						tc-= '0';
					else {
						seen= tc;
						break;
					}
					n= n*010 + tc;
					readtc();
				}
				tc= n;
			}
		}
		str[i++]= tc;
		if (i == len) str= allocate(str, (len*= 2) * sizeof(str[0]));

		if (seen < 0) readtc(); else tc= seen;
	}

	if (tc == quote) {
		readtc();
	} else {
		parse_err(1, nil, "string contains newline\n");
	}
	str[i]= 0;
	str= allocate(str, (i+1) * sizeof(str[0]));
	s= new_token();
	s->type= T_STRING;
	s->name= str;
	s->len= i;
	return s;
}

static int old_n= 0;		/* To speed up n, n+1, n+2, ... accesses. */
static token_t **old_ptq= &tq;

token_t *get_token(int n)
/* Return the n-th token on the input queue. */
{
	token_t *t, **ptq;

	assert(n >= 0);

	if (0 && n >= old_n) {
		/* Go forward from the previous point. */
		n-= old_n;
		old_n+= n;
		ptq= old_ptq;
	} else {
		/* Restart from the head of the queue. */
		old_n= n;
		ptq= &tq;
	}

	for (;;) {
		if ((t= *ptq) == nil) {
			/* Token queue doesn't have element <n>, read a
			 * new token from the input stream.
			 */
			while (isspace(tc) || iscomment(tc)) {
				if (iscomment(tc)) {
					while (tc != '\n' && tc != EOF)
						readtc();
				} else {
					readtc();
				}
			}

			if (tc == EOF) {
				t= new_token();
				t->type= T_EOF;
			} else
			if (isidentchar(tc)) {
				t= get_word();
			} else
			if (tc == '\'' || tc == '"') {
				t= get_string();
			} else {
				if (tc == '\n') tc= ';';
				t= new_token();
				t->type= T_CHAR;
				t->symbol= tc;
				readtc();
				if (t->symbol == '<' && tc == '<') {
					t->symbol= S_LEFTSHIFT;
					readtc();
				} else
				if (t->symbol == '>' && tc == '>') {
					t->symbol= S_RIGHTSHIFT;
					readtc();
				}
			}
			*ptq= t;
		}
		if (n == 0) break;
		n--;
		ptq= &t->next;
	}
	old_ptq= ptq;
	return t;
}

void skip_token(int n)
/* Remove n tokens from the input queue.  One is not allowed to skip unread
 * tokens.
 */
{
	token_t *junk;

	assert(n >= 0);

	while (n > 0) {
		assert(tq != nil);

		junk= tq;
		tq= tq->next;
		deallocate(junk->name);
		deallocate(junk);
		n--;
	}
	/* Reset the old reference. */
	old_n= 0;
	old_ptq= &tq;
}
