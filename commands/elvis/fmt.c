/* fmt.c */

/* usage: fmt [-width] [files]...
 *
 * Fmt rearrages text in order to make each line have roughly the
 * same width.  Indentation and word spacing is preserved.
 *
 * The default width is 72 characters, but you can override that via -width.
 * If no files are given on the command line, then it reads stdin.
 */

#include <stdio.h>

#ifndef TRUE
# define TRUE	1
# define FALSE	0
#endif



int	width = 72;	/* the desired line width */
int	isblank;	/* is the current output line blank? */
int	indent;		/* width of the indentation */
char	ind[512];	/* indentation text */
char	word[1024];	/* word buffer */

/* This function displays a usage message and quits */
void usage()
{
	fprintf(stderr, "usage: fmt [-width] [files]...\n");
	exit(2);
}



/* This function outputs a single word.  It takes care of spacing and the
 * newlines within a paragraph.
 */
void putword()
{
	int		i;		/* index into word[], or whatever */
	int		ww;		/* width of the word */
	int		sw;		/* width of spacing after word */
	static int	psw;		/* space width of previous word */
	static int	tab;		/* the width of text already written */


	/* separate the word and its spacing */
	for (ww = 0; word[ww] && word[ww] != ' '; ww++)
	{
	}
	sw = strlen(word) - ww;
	word[ww] = '\0';

	/* if no spacing (that is, the word was at the end of the line) then
	 * assume 1 space unless the last char of the word was punctuation
	 */
	if (sw == 0)
	{
		sw = 1;
		if (word[ww - 1] == '.' || word[ww - 1] == '?' || word[ww - 1] == '!')
			sw = 2;
	}

	/* if this is the first word on the line... */
	if (isblank)
	{
		/* output the indentation first */
		fputs(ind, stdout);
		tab = indent;
	}
	else /* text has already been written to this output line */
	{
		/* will the word fit on this line? */
		if (psw + ww + tab <= width)
		{
			/* yes - so write the previous word's spacing */
			for (i = 0; i < psw; i++)
			{
				putchar(' ');
			}
			tab += psw;
		}
		else
		{
			/* no, so write a newline and the indentation */
			putchar('\n');
			fputs(ind, stdout);
			tab = indent;
		}
	}

	/* write the word itself */
	fputs(word, stdout);
	tab += ww;

	/* remember this word's spacing */
	psw = sw;

	/* this output line isn't blank anymore. */
	isblank = FALSE;
}



/* This function reformats text. */
void fmt(in)
	FILE	*in;		/* the name of the input stream */
{
	int	ch;		/* character from input stream */
	int	prevch;		/* the previous character in the loop */
	int	i;		/* index into ind[] or word[] */
	int	inword;		/* boolean: are we between indent & newline? */


	/* for each character in the stream... */
	for (indent = -1, isblank = TRUE, inword = FALSE, i = 0, prevch = '\n';
	     (ch = getc(in)) != EOF;
	     prevch = ch)
	{
		/* is this the end of a line? */
		if (ch == '\n')
		{
			/* if end of last word in the input line */
			if (inword)
			{
				/* if it really is a word */
				if (i > 0)
				{
					/* output it */
					word[i] = '\0';
					putword();
				}
			}
			else /* blank line in input */
			{
				/* finish the previous paragraph */
				if (!isblank)
				{
					putchar('\n');
					isblank = TRUE;
				}

				/* output a blank line */
				putchar('\n');
			}

			/* continue with next input line... */
			indent = -1;
			i = 0;
			inword = FALSE;
			continue;
		}

		/* if we're expecting indentation now... */
		if (indent < 0)
		{
			/* if this is part of the indentation... */
			if (ch == ' ' || ch == '\t')
			{
				/* remember it */
				ind[i++] = ch;
			}
			else /* end of indentation */
			{
				/* mark the end of the indentation string */
				ind[i] = '\0';

				/* calculate the width of the indentation */
				for (i = indent = 0; ind[i]; i++)
				{
					if (ind[i] == '\t')
						indent = (indent | 7) + 1;
					else
						indent++;
				}

				/* reset the word index */
				i = 0;

				/* reprocess that last character */
				ungetc(ch, in);
			}

			/* continue in the for-loop */
			continue;
		}

		/* if we get here, we're either in a word or in the space
		 * after a word.
		 */
		inword = TRUE;

		/* is this the start of a new word? */
		if (ch != ' ' && prevch == ' ')
		{
			/* yes!  output the previous word */
			word[i] = '\0';
			putword();

			/* reset `i' to the start of the word[] buffer */
			i = 0;
		}
		word[i++] = ch;
	}

	/* if necessary, write a final newline */
	if (!isblank)
	{
		putchar('\n');
		isblank = TRUE;
	}
}





int main(argc, argv)
	int	argc;
	char	**argv;
{
	FILE	*in;	/* an input stream */
	int	error;	/* if non-zero, then an error occurred */
	int	i;


	/* handle the -width flag, if given */
	if (argc > 1 && argv[1][0] == '-')
	{
		width = atoi(argv[1] + 1);
		if (width <= 0)
		{
			usage();
		}
		argc--;
		argv++;
	}

	/* if no filenames given, then process stdin */
	if (argc == 1)
	{
		fmt(stdin);
	}
	else /* one or more filenames given */
	{
		for (error = 0, i = 1; i < argc; i++)
		{
			in = fopen(argv[i], "r");
			if (!in)
			{
				perror(argv[i]);
				error = 3;
			}
			else
			{
				fmt(in);
				fclose(in);
			}
		}
	}

	/* exit, possibly indicating an error */
	exit(error);
	/*NOTREACHED*/
}
