/* ctags.c */

/* This is a reimplementation of the ctags(1) program.  It supports ANSI C,
 * and has heaps o' flags.  It is meant to be distributed with elvis.
 */

#include <stdio.h>
#include "config.h"
#ifndef FALSE
# define FALSE	0
# define TRUE	1
#endif
#ifndef TAGS
# define TAGS	"tags"
#endif
#ifndef REFS
# define REFS	"refs"
#endif
#ifndef BLKSIZE
# define BLKSIZE 1024
#endif

#include "ctype.c" /* yes, that really is the .c file, not the .h one. */

/* -------------------------------------------------------------------------- */
/* Some global variables */

/* The following boolean variables are set according to command line flags */
int	incl_static;	/* -s  include static tags */
int	incl_types;	/* -t  include typedefs and structs */
int	incl_vars;	/* -v  include variables */
int	make_refs;	/* -r  generate a "refs" file */
int	append_files;	/* -a  append to "tags" [and "refs"] files */

/* The following are used for outputting to the "tags" and "refs" files */
FILE	*tags;		/* used for writing to the "tags" file */
FILE	*refs;		/* used for writing to the "refs" file */

/* -------------------------------------------------------------------------- */
/* These are used for reading a source file.  It keeps track of line numbers */
char	*file_name;	/* name of the current file */
FILE	*file_fp;	/* stream used for reading the file */
long	file_lnum;	/* line number in the current file */
long	file_seek;	/* fseek() offset to the start of current line */
int	file_afternl;	/* boolean: was previous character a newline? */
int	file_prevch;	/* a single character that was ungotten */
int	file_header;	/* boolean: is the current file a header file? */

/* This function opens a file, and resets the line counter.  If it fails, it
 * it will display an error message and leave the file_fp set to NULL.
 */
void file_open(name)
	char	*name;	/* name of file to be opened */
{
	/* if another file was already open, then close it */
	if (file_fp)
	{
		fclose(file_fp);
	}

	/* try to open the file for reading.  The file must be opened in
	 * "binary" mode because otherwise fseek() would misbehave under DOS.
	 */
#if MSDOS || TOS
	file_fp = fopen(name, "rb");
#else
	file_fp = fopen(name, "r");
#endif
	if (!file_fp)
	{
		perror(name);
	}

	/* reset the name & line number */
	file_name = name;
	file_lnum = 0L;
	file_seek = 0L;
	file_afternl = TRUE;

	/* determine whether this is a header file */
	file_header = FALSE;
	name += strlen(name) - 2;
	if (name >= file_name && name[0] == '.' && (name[1] == 'h' || name[1] == 'H'))
	{
		file_header = TRUE;
	}
}

/* This function reads a single character from the stream.  If the *previous*
 * character was a newline, then it also increments file_lnum and sets
 * file_offset.
 */
int file_getc()
{
	int	ch;

	/* if there is an ungotten character, then return it.  Don't do any
	 * other processing on it, though, because we already did that the
	 * first time it was read.
	 */
	if (file_prevch)
	{
		ch = file_prevch;
		file_prevch = 0;
		return ch;
	}

	/* if previous character was a newline, then we're starting a line */
	if (file_afternl)
	{
		file_afternl = FALSE;
		file_seek = ftell(file_fp);
		file_lnum++;
	}

	/* Get a character.  If no file is open, then return EOF */
	ch = (file_fp ? getc(file_fp) : EOF);

	/* if it is a newline, then remember that fact */
	if (ch == '\n')
	{
		file_afternl = TRUE;
	}

	/* return the character */
	return ch;
}

/* This function ungets a character from the current source file */
void file_ungetc(ch)
	int	ch;	/* character to be ungotten */
{
	file_prevch = ch;
}

/* This function copies the current line out some other fp.  It has no effect
 * on the file_getc() function.  During copying, any '\' characters are doubled
 * and a leading '^' or trailing '$' is also quoted.  The newline character is
 * not copied.
 *
 * This is meant to be used when generating a tag line.
 */
void file_copyline(seek, fp)
	long	seek;	/* where the lines starts in the source file */
	FILE	*fp;	/* the output stream to copy it to */
{
	long	oldseek;/* where the file's pointer was before we messed it up */
	char	ch;	/* a single character from the file */
	char	next;	/* the next character from this file */

	/* go to the start of the line */
	oldseek = ftell(file_fp);
	fseek(file_fp, seek, 0);

	/* if first character is '^', then emit \^ */
	ch = getc(file_fp);
	if (ch == '^')
	{
		putc('\\', fp);
		putc('^', fp);
		ch = getc(file_fp);
	}

	/* write everything up to, but not including, the newline */
	while (ch != '\n')
	{
		/* preread the next character from this file */
		next = getc(file_fp);

		/* if character is '\', or a terminal '$', then quote it */
		if (ch == '\\' || (ch == '$' && next == '\n'))
		{
			putc('\\', fp);
		}
		putc(ch, fp);

		/* next character... */
		ch = next;
	}

	/* seek back to the old position */
	fseek(file_fp, oldseek, 0);
}

/* -------------------------------------------------------------------------- */
/* This section handles preprocessor directives.  It strips out all of the
 * directives, and may emit a tag for #define directives.
 */

int	cpp_afternl;	/* boolean: look for '#' character? */
int	cpp_prevch;	/* an ungotten character, if any */
int	cpp_refsok;	/* boolean: can we echo characters out to "refs"? */

/* This function opens the file & resets variables */
void cpp_open(name)
	char	*name;	/* name of source file to be opened */
{
	/* use the lower-level file_open function to open the file */
	file_open(name);

	/* reset variables */
	cpp_afternl = TRUE;
	cpp_refsok = TRUE;
}

/* This function copies a character from the source file to the "refs" file */
void cpp_echo(ch)
	int	ch; /* the character to copy */
{
	static	wasnl;

	/* echo non-EOF chars, unless not making "ref", or echo turned off */
	if (ch != EOF && make_refs && cpp_refsok && !file_header)
	{
		/* try to avoid blank lines */
		if (ch == '\n')
		{
			if (wasnl)
			{
				return;
			}
			wasnl = TRUE;
		}
		else
		{
			wasnl = FALSE;
		}

		/* add the character */
		putc(ch, refs);
	}
}

/* This function returns the next character which isn't part of a directive */
int cpp_getc()
{
	static
	int	ch;	/* the next input character */
	char	*scan;

	/* if we have an ungotten character, then return it */
	if (cpp_prevch)
	{
		ch = cpp_prevch;
		cpp_prevch = 0;
		return ch;
	}

	/* Get a character from the file.  Return it if not special '#' */
	ch = file_getc();
	if (ch == '\n')
	{
		cpp_afternl = TRUE;
		cpp_echo(ch);
		return ch;
	}
	else if (ch != '#' || !cpp_afternl)
	{
		/* normal character.  Any non-whitespace should turn off afternl */
		if (ch != ' ' && ch != '\t')
		{
			cpp_afternl = FALSE;
		}
		cpp_echo(ch);
		return ch;
	}

	/* Yikes!  We found a directive */

	/* see whether this is a #define line */
	scan = " define ";
	while (*scan)
	{
		if (*scan == ' ')
		{
			/* space character matches any whitespace */
			do
			{
				ch = file_getc();
			} while (ch == ' ' || ch == '\t');
			file_ungetc(ch);
		}
		else
		{
			/* other characters should match exactly */
			ch = file_getc();
			if (ch != *scan)
			{
				file_ungetc(ch);
				break;
			}
		}
		scan++;
	}

	/* is this a #define line?  and should we generate a tag for it? */
	if (!*scan && (file_header || incl_static))
	{
		/* if not a header, then this will be a static tag */
		if (!file_header)
		{
			fputs(file_name, tags);
			putc(':', tags);
		}

		/* output the tag name */
		for (ch = file_getc(); isalnum(ch) || ch == '_'; ch = file_getc())
		{
			putc(ch, tags);
		}

		/* output a tab, the filename, another tab, and the line number */
		fprintf(tags, "\t%s\t%ld\n", file_name, file_lnum);
	}

	/* skip to the end of the directive -- a newline that isn't preceded
	 * by a '\' character.
	 */
	while (ch != EOF && ch != '\n')
	{
		if (ch == '\\')
		{
			ch = file_getc();
		}
		ch = file_getc();
	}

	/* return the newline that we found at the end of the directive */
	cpp_echo(ch);
	return ch;
}

/* This puts a character back into the input queue for the source file */
cpp_ungetc(ch)
	int	ch;	/* a character to be ungotten */
{
	cpp_prevch = ch;
}


/* -------------------------------------------------------------------------- */
/* This is the lexical analyser.  It gets characters from the preprocessor,
 * and gives tokens to the parser.  Some special codes are...
 *   (deleted)  /*...* /	(comments)
 *   (deleted)	//...\n	(comments)
 *   (deleted)	(*	(parens used in complex declaration)
 *   (deleted)	[...]	(array subscript, when ... contains no ])
 *   (deleted)	struct	(intro to structure declaration)
 *   BODY	{...}	('{' can occur anywhere, '}' only at BOW if ... has '{')
 *   ARGS	(...{	(args of function, not extern or forward)
 *   ARGS	(...);	(args of an extern/forward function declaration)
 *   COMMA	,	(separate declarations that have same scope)
 *   SEMICOLON	;	(separate declarations that have different scope)
 *   SEMICOLON  =...;	(initializer)
 *   TYPEDEF	typedef	(the "typedef" keyword)
 *   STATIC	static	(the "static" keyword)
 *   STATIC	private	(the "static" keyword)
 *   STATIC	PRIVATE	(the "static" keyword)
 *   NAME	[a-z]+	(really any valid name that isn't reserved word)
 */

/* #define EOF -1 */
#define DELETED	  0
#define BODY	  1
#define ARGS	  2
#define COMMA	  3
#define SEMICOLON 4
#define TYPEDEF   5
#define STATIC	  6
#define EXTERN	  7
#define NAME	  8

char	lex_name[BLKSIZE];	/* the name of a "NAME" token */
long	lex_seek;		/* start of line that contains lex_name */

lex_gettoken()
{
	int	ch;		/* a character from the preprocessor */
	int	next;		/* the next character */
	int	token;		/* the token that we'll return */
	int	i;

	/* loop until we get a token that isn't "DELETED" */
	do
	{
		/* get the next character */
		ch = cpp_getc();

		/* process the character */
		switch (ch)
		{
		  case ',':
			token = COMMA;
			break;

		  case ';':
			token = SEMICOLON;
			break;

		  case '/':
			/* get the next character */
			ch = cpp_getc();
			switch (ch)
			{
			  case '*':	/* start of C comment */
				ch = cpp_getc();
				next = cpp_getc();
				while (next != EOF && (ch != '*' || next != '/'))
				{
					ch = next;
					next = cpp_getc();
				}
				break;

			  case '/':	/* start of a C++ comment */
				do
				{
					ch = cpp_getc();
				} while (ch != '\n' && ch != EOF);
				break;

			  default:	/* some other slash */
				cpp_ungetc(ch);
			}
			token = DELETED;
			break;

		  case '(':
			ch = cpp_getc();
			if (ch == '*')
			{
				token = DELETED;
			}
			else
			{
				next = cpp_getc();
				while (ch != '{' && ch != EOF && (ch != ')' || next != ';'))/*}*/
				{
					ch = next;
					next = cpp_getc();
				}
				if (ch == '{')/*}*/
				{
					cpp_ungetc(ch);
				}
				else if (next == ';')
				{
					cpp_ungetc(next);
				}
				token = ARGS;
			}
			break;

		  case '{':/*}*/
			/* don't send the next characters to "refs" */
			cpp_refsok = FALSE;

			/* skip ahead to closing '}', or to embedded '{' */
			do
			{
				ch = cpp_getc();
			} while (ch != '{' && ch != '}' && ch != EOF);

			/* if has embedded '{', then skip to '}' in column 1 */
			if (ch == '{') /*}*/
			{
				ch = cpp_getc();
				next = cpp_getc();
				while (ch != EOF && (ch != '\n' || next != '}'))/*{*/
				{
					ch = next;
					next = cpp_getc();
				}
			}

			/* resume "refs" processing */
			cpp_refsok = TRUE;
			cpp_echo('}');

			token = BODY;
			break;

		  case '[':
			/* skip to matching ']' */
			do
			{
				ch = cpp_getc();
			} while (ch != ']' && ch != EOF);
			token = DELETED;
			break;

		  case '=':
		  	/* skip to next ';' */
			do
			{
				ch = cpp_getc();

				/* leave array initializers out of "refs" */
				if (ch == '{')
				{
					cpp_refsok = FALSE;
				}
			} while (ch != ';' && ch != EOF);

			/* resume echoing to "refs" */
			if (!cpp_refsok)
			{
				cpp_refsok = TRUE;
				cpp_echo('}');
				cpp_echo(';');
			}
			token = SEMICOLON;
			break;

		  case EOF:
			token = EOF;
			break;

		  default:
			/* is this the start of a name/keyword? */
			if (isalpha(ch) || ch == '_')
			{
				/* collect the whole word */
				lex_name[0] = ch;
				for (i = 1, ch = cpp_getc();
				     i < BLKSIZE - 1 && (isalnum(ch) || ch == '_');
				     i++, ch = cpp_getc())
				{
					lex_name[i] = ch;
				}
				lex_name[i] = '\0';
				cpp_ungetc(ch);

				/* is it a reserved word? */
				if (!strcmp(lex_name, "typedef"))
				{
					token = TYPEDEF;
					lex_seek = -1L;
				}
				else if (!strcmp(lex_name, "static")
				      || !strcmp(lex_name, "private")
				      || !strcmp(lex_name, "PRIVATE"))
				{
					token = STATIC;
					lex_seek = -1L;
				}
				else if (!strcmp(lex_name, "extern")
				      || !strcmp(lex_name, "EXTERN")
				      || !strcmp(lex_name, "FORWARD"))
				{
					token = EXTERN;
					lex_seek = -1L;
				}
				else
				{
					token = NAME;
					lex_seek = file_seek;
				}
			}
			else /* not part of a name/keyword */
			{
				token = DELETED;
			}

		} /* end switch(ch) */

	} while (token == DELETED);

	return token;
}

/* -------------------------------------------------------------------------- */
/* This is the parser.  It locates tag candidates, and then decides whether to
 * generate a tag for them.
 */

/* This function generates a tag for the object in lex_name, whose tag line is
 * located at a given seek offset.
 */
void maketag(scope, seek)
	int	scope;	/* 0 if global, or STATIC if static */
	long	seek;	/* the seek offset of the line */
{
	/* output the tagname and filename fields */
	if (scope == EXTERN)
	{
		/* whoa!  we should *never* output a tag for "extern" decl */
		return;
	}
	else if (scope == STATIC)
	{
		fprintf(tags, "%s:%s\t%s\t", file_name, lex_name, file_name);
	}
	else
	{
		fprintf(tags, "%s\t%s\t", lex_name, file_name);
	}

	/* output the target line */
	putc('/', tags);
	putc('^', tags);
	file_copyline(seek, tags);
	putc('$', tags);
	putc('/', tags);
	putc('\n', tags);
}


/* This function parses a source file, adding any tags that it finds */
void ctags(name)
	char	*name;	/* the name of a source file to be checked */
{
	int	prev;	/* the previous token from the source file */
	int	token;	/* the current token from the source file */
	int	scope;	/* normally 0, but could be a TYPEDEF or STATIC token */
	int	gotname;/* boolean: does lex_name contain a tag candidate? */
	long	tagseek;/* start of line that contains lex_name */

	/* open the file */
	cpp_open(name);

	/* reset */
	scope = 0;
	gotname = FALSE;
	token = SEMICOLON;

	/* parse until the end of the file */
	while (prev = token, (token = lex_gettoken()) != EOF)
	{
		/* scope keyword? */
		if (token == TYPEDEF || token == STATIC || token == EXTERN)
		{
			scope = token;
			gotname = FALSE;
			continue;
		}

		/* name of a possible tag candidate? */
		if (token == NAME)
		{
			tagseek = file_seek;
			gotname = TRUE;
			continue;
		}

		/* if NAME BODY, without ARGS, then NAME is a struct tag */
		if (gotname && token == BODY && prev != ARGS)
		{
			gotname = FALSE;
			
			/* ignore if in typedef -- better name is coming soon */
			if (scope == TYPEDEF)
			{
				continue;
			}

			/* generate a tag, if -t and maybe -s */
			if (incl_types && (file_header || incl_static))
			{
				maketag(file_header ? 0 : STATIC, tagseek);
			}
		}

		/* If NAME ARGS BODY, then NAME is a function */
		if (gotname && prev == ARGS && token == BODY)
		{
			gotname = FALSE;
			
			/* generate a tag, maybe checking -s */
			if (scope != STATIC || incl_static)
			{
				maketag(scope, tagseek);
			}
		}

		/* If NAME SEMICOLON or NAME COMMA, then NAME is var/typedef */
		if (gotname && (token == SEMICOLON || token == COMMA))
		{
			gotname = FALSE;

			/* generate a tag, if -v/-t and maybe -s */
			if (scope == TYPEDEF && incl_types && (file_header || incl_static)
			 || scope == STATIC && incl_vars && incl_static
			 || incl_vars)
			{
				/* a TYPEDEF outside of a header is STATIC */
				if (scope == TYPEDEF && !file_header)
				{
					maketag(STATIC, tagseek);
				}
				else /* use whatever scope was declared */
				{
					maketag(scope, tagseek);
				}
			}
		}

		/* reset after a semicolon or ARGS BODY pair */
		if (token == SEMICOLON || (prev == ARGS && token == BODY))
		{
			scope = 0;
			gotname = FALSE;
		}
	}

	/* The source file will be automatically closed */
}

/* -------------------------------------------------------------------------- */

void usage()
{
	fprintf(stderr, "usage: ctags [flags] filenames...\n");
	fprintf(stderr, "\t-s  include static functions\n");
	fprintf(stderr, "\t-t  include typedefs\n");
	fprintf(stderr, "\t-v  include variable declarations\n");
	fprintf(stderr, "\t-r  generate a \"refs\" file, too\n");
	fprintf(stderr, "\t-a  append to \"tags\", instead of overwriting\n");
	exit(2);
}



#if AMIGA
# include "amiwild.c"
#endif

#if VMS
# include "vmswild.c"
#endif

main(argc, argv)
	int	argc;
	char	**argv;
{
	int	i, j;

#if MSDOS || TOS
	char	**wildexpand();
	argv = wildexpand(&argc, argv);
#endif

	/* build the tables used by the ctype macros */
	_ct_init("");

	/* parse the option flags */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
	{
		for (j = 1; argv[i][j]; j++)
		{
			switch (argv[i][j])
			{
			  case 's':	incl_static = TRUE;	break;
			  case 't':	incl_types = TRUE;	break;
			  case 'v':	incl_vars = TRUE;	break;
			  case 'r':	make_refs = TRUE;	break;
			  case 'a':	append_files = TRUE;	break;
			  default:	usage();
			}
		}
	}

	/* There should always be at least one source file named in args */
	if (i == argc)
	{
		usage();
	}

	/* open the "tags" and maybe "refs" files */
	tags = fopen(TAGS, append_files ? "a" : "w");
	if (!tags)
	{
		perror(TAGS);
		exit(3);
	}
	if (make_refs)
	{
		refs = fopen(REFS, append_files ? "a" : "w");
		if (!refs)
		{
			perror(REFS);
			exit(4);
		}
	}

	/* parse each source file */
	for (; i < argc; i++)
	{
		ctags(argv[i]);
	}

	/* close "tags" and maybe "refs" */
	fclose(tags);
	if (make_refs)
	{
		fclose(refs);
	}

#ifdef SORT
		/* This is a hack which will sort the tags list.   It should
		 * on UNIX and OS-9.  You may have trouble with csh.   Note
		 * that the tags list only has to be sorted if you intend to
		 * use it with the real vi;  elvis permits unsorted tags.
		 */
# if OSK
		system("qsort tags >-_tags; -nx; del tags; rename _tags tags");
# else	
		system("sort tags >_tags$$; mv _tags$$ tags");
# endif
#endif

	exit(0);
	/*NOTREACHED*/
}

#if MSDOS || TOS
# define WILDCARD_NO_MAIN
# include "wildcard.c"
#endif
