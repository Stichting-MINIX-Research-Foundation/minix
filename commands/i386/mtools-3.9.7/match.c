/*
 * Do shell-style pattern matching for '?', '\', '[..]', and '*' wildcards.
 * Returns 1 if match, 0 if not.
 */

#include "sysincludes.h"
#include "mtools.h"


static int casecmp(char a,char b)
{
	return toupper(a) == toupper(b);
}

static int exactcmp(char a,char b)
{
	return a == b;
}


static int parse_range(const char **p, const char *s, char *out, 
		       int (*compfn)(char a, char b))
{
	char table[256];
	int reverse;
	int i;
	short first, last;

	if (**p == '^') {
		reverse = 1;
		(*p)++;
	} else
		reverse=0;	
	for(i=0; i<256; i++)
		table[i]=0;
	while(**p != ']') {
		if(!**p)
			return 0;
		if((*p)[1] == '-') {
			first = **p;
			(*p)+=2;
			if(**p == ']')
				last = 256;
			else
				last = *((*p)++);				
			for(i=first; i<last; i++)
				table[i] = 1;
		} else
			table[(int) *((*p)++)] = 1;
	}
	if(out)
		*out = *s;
	if(table[(int) *s])
		return 1 ^ reverse;
	if(compfn == exactcmp)
		return reverse;
	if(table[tolower(*s)]) {
		if(out)
			*out = tolower(*s);
		return 1 ^ reverse;
	}
	if(table[toupper(*s)]) {
		if(out)
			*out = toupper(*s);
		return 1 ^ reverse;
	}
	return reverse;
}


static int _match(const char *s, const char *p, char *out, int Case,
		  int length,
		  int (*compfn) (char a, char b))
{
	for (; *p != '\0' && length; ) {
		switch (*p) {
			case '?':	/* match any one character */
				if (*s == '\0')
					return(0);
				if(out)
					*(out++) = *s;
				break;
			case '*':	/* match everything */
				while (*p == '*' && length) {
					p++;
					length--;
				}

					/* search for next char in pattern */
				while(*s) {
					if(_match(s, p, out, Case, length, 
						  compfn))
						return 1;
					if(out)
						*out++ = *s;
					s++;
				}
				continue;
			case '[':	 /* match range of characters */
				p++;
				length--;
				if(!parse_range(&p, s, out++, compfn))
					return 0;
				break;
			case '\\':	/* Literal match with next character */
				p++;
				length--;
				/* fall thru */
			default:
				if (!compfn(*s,*p))
					return(0);
				if(out)
					*(out++) = *p;
				break;
		}
		p++;
		length--;
		s++;
	}
	if(out)
		*out = '\0';

					/* string ended prematurely ? */
	if (*s != '\0')
		return(0);
	else
		return(1);
}


int match(const char *s, const char *p, char *out, int Case, int length)
{
	int (*compfn)(char a, char b);

	if(Case)
		compfn = casecmp;
	else
		/*compfn = exactcmp;*/
		compfn = casecmp;
	return _match(s, p, out, Case, length, compfn);
}

