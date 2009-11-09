/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	Access to program arguments and environment
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/

extern char **argv, ***_penviron;
extern int argc;
unsigned int _Arguments__Argc;

static char *
findname(s1, s2)
register char *s1, *s2;
{

	while (*s1 == *s2++) s1++;
	if (*s1 == '\0' && *(s2-1) == '=') return s2;
	return 0;
}

static unsigned int
scopy(src, dst, max)
	register char *src, *dst;
	unsigned int max;
{
	register unsigned int i = 0;

	while (*src && i <= max) {
		i++;
		*dst++ = *src++;
	}
	if (i <= max) {
		*dst = '\0';
		return i+1;
	}
	while (*src++) i++;
	return i + 1;
}

_Arguments_()
{
	_Arguments__Argc = argc;
}

unsigned
_Arguments__Argv(n, argument, l, u, s)
	unsigned int u;
	char *argument;
{

	if (n >= argc) return 0;
	return scopy(argv[n], argument, u);
}

unsigned
_Arguments__GetEnv(name, nn, nu, ns, value, l, u, s)
	char *name, *value;
	unsigned int nu, u;
{
	register char **p = *_penviron;
	register char *v = 0;

	while (*p && !(v = findname(name, *p++))) {
		/* nothing */
	}
	if (!v) return 0;
	return scopy(v, value, u);
}
