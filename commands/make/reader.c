/*************************************************************************
 *
 *  m a k e :   r e a d e r . c
 *
 *  Read in makefile
 *========================================================================
 * Edition history
 *
 *  #    Date                         Comments                       By
 * --- -------- ---------------------------------------------------- ---
 *   1    ??                                                         ??
 *   2 23.08.89 cast to NULL added                                   RAL
 *   3 30.08.89 indention changed                                    PSH,RAL
 *   4 03.09.89 fixed LZ eliminated                                  RAL
 * ------------ Version 2.0 released ------------------------------- RAL
 *
 *************************************************************************/

#include "h.h"


/*
 *	Syntax error handler.  Print message, with line number, and exits.
 */
void error(msg, a1)
char *msg;
char *a1;
{
  fprintf(stderr, "%s: ", myname);
  fprintf(stderr, msg, a1);
  if (lineno)  fprintf(stderr, " in %s near line %d", makefile, lineno);
  fputc('\n', stderr);
  exit(1);
}


/*
 *	Read a line into the supplied string.  Remove
 *	comments, ignore blank lines. Deal with	quoted (\) #, and
 *	quoted newlines.  If EOF return TRUE.
 *
 *	The comment handling code has been changed to leave comments and
 *	backslashes alone in shell commands (lines starting with a tab).
 *	This is not what POSIX wants, but what all makes do.  (KJB)
 */
bool getline(strs, fd)
struct str *strs;
FILE *fd;
{
  register char *p;
  char          *q;
  int            c;

  for (;;) {
	strs->pos = 0;
	for (;;) {
		do {
			if (strs->pos >= strs->len)
				strrealloc(strs);
			if ((c = getc(fd)) == EOF)
				return TRUE;		/* EOF */
			(*strs->ptr)[strs->pos++] = c;
		} while (c != '\n');

		lineno++;

		if (strs->pos >= 2 && (*strs->ptr)[strs->pos - 2] == '\\') {
			(*strs->ptr)[strs->pos - 2] = '\n';
			strs->pos--;
		} else {
			break;
		}
	}

	if (strs->pos >= strs->len)
		strrealloc(strs);
	(*strs->ptr)[strs->pos] = '\0';

	p = q =  *strs->ptr;
	while (isspace(*q)) q++;
	if (*p != '\t' || *q == '#') {
		while (((q = strchr(p, '#')) != (char *)0) &&
		    (p != q) && (q[-1] == '\\'))
		{
			char	*a;

			a = q - 1;	/*  Del \ chr; move rest back  */
			p = q;
			while (*a++ = *q++)
				;
		}
		if (q != (char *)0)
			{
			q[0] = '\n';
			q[1] = '\0';
		}
	}

	p = *strs->ptr;
	while (isspace(*p))	/*  Checking for blank  */
		p++;

	if (*p != '\0')
		return FALSE;
  }
}


/*
 *	Get a word from the current line, surounded by white space.
 *	return a pointer to it. String returned has no white spaces
 *	in it.
 */
char  *gettok(ptr)
register char **ptr;
{
  register char *p;


  while (isspace(**ptr))	/*  Skip spaces  */
	(*ptr)++;

  if (**ptr == '\0')	/*  Nothing after spaces  */
	return ((char *)NULL);

  p = *ptr;		/*  word starts here  */

  while ((**ptr != '\0') && (!isspace(**ptr)))
	(*ptr)++;	/*  Find end of word  */

  *(*ptr)++ = '\0';	/*  Terminate it  */

  return(p);
}
