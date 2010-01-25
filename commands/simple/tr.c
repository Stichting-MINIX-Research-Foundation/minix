/* tr - translate characters		Author: Michiel Huisjes */
/* Usage: tr [-cds] [string1 [string2]]
 *	c: take complement of string1
 *	d: delete input characters coded string1
 *	s: squeeze multiple output characters of string2 into one character
 */

#define BUFFER_SIZE	1024
#define ASCII		0377

typedef char BOOL;
#define TRUE	1
#define FALSE	0

#define NIL_PTR		((char *) 0)

BOOL com_fl, del_fl, sq_fl;

unsigned char output[BUFFER_SIZE], input[BUFFER_SIZE];
unsigned char vector[ASCII + 1];
BOOL invec[ASCII + 1], outvec[ASCII + 1];

short in_index, out_index;

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void convert, (void));
_PROTOTYPE(void map, (unsigned char *string1, unsigned char *string2));
_PROTOTYPE(void expand, (char *arg, unsigned char *buffer));
_PROTOTYPE(void complement, (unsigned char *buffer));

int main(argc, argv)
int argc;
char *argv[];
{
  register unsigned char *ptr;
  int index = 1;
  short i;

  if (argc > 1 && argv[index][0] == '-') {
	for (ptr = (unsigned char *) &argv[index][1]; *ptr; ptr++) {
		switch (*ptr) {
		    case 'c':	com_fl = TRUE;	break;
		    case 'd':	del_fl = TRUE;	break;
		    case 's':	sq_fl = TRUE;	break;
		    default:
			write(2,"Usage: tr [-cds] [string1 [string2]].\n", 38);
			exit(1);
		}
	}
	index++;
  }
  for (i = 0; i <= ASCII; i++) {
	vector[i] = i;
	invec[i] = outvec[i] = FALSE;
  }

  if (argv[index] != NIL_PTR) {
	expand(argv[index++], input);
	if (com_fl) complement(input);
	if (argv[index] != NIL_PTR) expand(argv[index], output);
	if (argv[index] != NIL_PTR) map(input, output);
	for (ptr = input; *ptr; ptr++) invec[*ptr] = TRUE;
	for (ptr = output; *ptr; ptr++) outvec[*ptr] = TRUE;
  }
  convert();
  return(0);
}

void convert()
{
  short read_chars = 0;
  short c, coded;
  short last = -1;

  for (;;) {
	if (in_index == read_chars) {
		if ((read_chars = read(0, (char *)input, BUFFER_SIZE)) <= 0) {
			if (write(1, (char *)output, out_index) != out_index)
				write(2, "Bad write\n", 10);
			exit(0);
		}
		in_index = 0;
	}
	c = input[in_index++];
	coded = vector[c];
	if (del_fl && invec[c]) continue;
	if (sq_fl && last == coded && outvec[coded]) continue;
	output[out_index++] = last = coded;
	if (out_index == BUFFER_SIZE) {
		if (write(1, (char *)output, out_index) != out_index) {
			write(2, "Bad write\n", 10);
			exit(1);
		}
		out_index = 0;
	}
  }

  /* NOTREACHED */
}

void map(string1, string2)
register unsigned char *string1, *string2;
{
  unsigned char last;

  while (*string1) {
	if (*string2 == '\0')
		vector[*string1] = last;
	else
		vector[*string1] = last = *string2++;
	string1++;
  }
}

static int starts_with(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2)
	{
		s1++;
		s2++;
	}
	return *s1 == 0;
}

/* 
 * character classes from 
 * http://www.opengroup.org/onlinepubs/009695399/utilities/tr.html
 * missing: blank, punct, cntrl, graph, print, space 
 */
static struct
{
	const char *keyword;
	char first;
	char last;
} expand_keywords[] = {
	{ "[:alnum:]", 'A', 'Z' },
	{ "[:alnum:]", 'a', 'z' },
	{ "[:alnum:]", '0', '9' },
	{ "[:alpha:]", 'A', 'Z' },
	{ "[:alpha:]", 'a', 'z' },
	{ "[:digit:]", '0', '9' },
	{ "[:lower:]", 'a', 'z' },
	{ "[:upper:]", 'A', 'Z' },
	{ "[:xdigit:]", '0', '9' },		
	{ "[:xdigit:]", 'A', 'F' },
	{ "[:xdigit:]", 'a', 'f' }
};

#define LENGTH(a) ((sizeof((a))) / (sizeof((a)[0])))

void expand(arg, buffer)
register char *arg;
register unsigned char *buffer;
{
  int i, ac, keyword_index;

  while (*arg) {
	if (*arg == '\\') {
		arg++;
		i = ac = 0;
		if (*arg >= '0' && *arg <= '7') {
			do {
				ac = (ac << 3) + *arg++ - '0';
				i++;
			} while (i < 4 && *arg >= '0' && *arg <= '7');
			*buffer++ = ac;
		} else if (*arg != '\0')
			*buffer++ = *arg++;
	} else if (*arg == '[') {
		/* does one of the keywords match? */
		keyword_index = -1;
		for (i = 0; i < LENGTH(expand_keywords); i++)
			if (starts_with(expand_keywords[i].keyword, arg))
			{
				/* we have a match, remember and expand */
				keyword_index = i;
				ac = expand_keywords[i].first;
				while (ac <= expand_keywords[i].last)
					*buffer++ = ac++;
			}

		/* skip keyword if found, otherwise expand range */
		if (keyword_index >= 0)
			arg += strlen(expand_keywords[keyword_index].keyword);
		else
		{
			/* expand range */
			arg++;
			i = *arg++;
			if (*arg++ != '-') {
				*buffer++ = '[';
				arg -= 2;
				continue;
			}
			ac = *arg++;
			while (i <= ac) *buffer++ = i++;
			arg++;		/* Skip ']' */
		}
	} else
		*buffer++ = *arg++;
  }
}

void complement(buffer)
unsigned char *buffer;
{
  register unsigned char *ptr;
  register short i, index;
  unsigned char conv[ASCII + 2];

  index = 0;
  for (i = 1; i <= ASCII; i++) {
	for (ptr = buffer; *ptr; ptr++)
		if (*ptr == i) break;
	if (*ptr == '\0') conv[index++] = i & ASCII;
  }
  conv[index] = '\0';
  strcpy((char *)buffer, (char *)conv);
}
