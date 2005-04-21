/*
strcasecmp.c

Created Oct 14, 1991 by Philip Homburg
*/

#include <ctype.h>
#include <string.h>

#ifdef __STDC__
#define _CONST	const
#else
#define _CONST
#endif

int
strcasecmp(s1, s2)
_CONST char *s1, *s2;
{
	int c1, c2;
	while (c1= toupper(*s1++), c2= toupper(*s2++), c1 == c2 && (c1 & c2))
		;
	if (c1 & c2)
		return c1 < c2 ? -1 : 1;
	return c1 ? 1 : (c2 ? -1 : 0);
}

int
strncasecmp(s1, s2, len)
_CONST char *s1, *s2;
size_t len;
{
	int c1, c2;
	do {
		if (len == 0)
			return 0;
		len--;
	} while (c1= toupper(*s1++), c2= toupper(*s2++), c1 == c2 && (c1 & c2))
		;
	if (c1 & c2)
		return c1 < c2 ? -1 : 1;
	return c1 ? 1 : (c2 ? -1 : 0);
}
