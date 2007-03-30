#

/*LINTLIBRARY*/

#include <string.h>
#include <stdlib.h>

#include "stdio.h"

#ifndef lint
#ifndef NOID
static char	sccsid[] = "@(#)ialloc.c	7.14";
#endif /* !NOID */
#endif /* !lint */

#ifndef alloc_t
#define alloc_t	unsigned
#endif /* !alloc_t */

#ifdef MAL
#define NULLMAL(x)	((x) == NULL || (x) == MAL)
#else /* !MAL */
#define NULLMAL(x)	((x) == NULL)
#endif /* !MAL */

#if 0
extern char *	calloc();
extern char *	malloc();
extern char *	realloc();
extern char *	strcpy();
#endif

char *
imalloc(int n)
{
#ifdef MAL
	register char *	result;

	if (n == 0)
		n = 1;
	result = malloc((alloc_t) n);
	return (result == MAL) ? NULL : result;
#else /* !MAL */
	if (n == 0)
		n = 1;
	return malloc((alloc_t) n);
#endif /* !MAL */
}

char *
icalloc(int nelem, int elsize)
{
	if (nelem == 0 || elsize == 0)
		nelem = elsize = 1;
	return calloc((alloc_t) nelem, (alloc_t) elsize);
}

char *
irealloc(char *pointer, int size)
{
	if (NULLMAL(pointer))
		return imalloc(size);
	if (size == 0)
		size = 1;
	return realloc(pointer, (alloc_t) size);
}

char *
icatalloc(char *old, char *new)
{
	register char *	result;
	register	oldsize, newsize;

	oldsize = NULLMAL(old) ? 0 : strlen(old);
	newsize = NULLMAL(new) ? 0 : strlen(new);
	if ((result = irealloc(old, oldsize + newsize + 1)) != NULL)
		if (!NULLMAL(new))
			(void) strcpy(result + oldsize, new);
	return result;
}

char *
icpyalloc(char *string)
{
	return icatalloc((char *) NULL, string);
}

ifree(char *p)
{
	if (!NULLMAL(p))
		free(p);
}

icfree(char *p)
{
	if (!NULLMAL(p))
		free(p);
}
