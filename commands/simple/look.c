/*	look 1.3 - Find lines in a sorted list.		Author: Kees J. Bot
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char DEFAULT[] = "/usr/lib/dict/words";

char *string, *wordlist= DEFAULT;

#define MAXLEN	1024	/* Maximum word length. */

int dflg= 0, fflg= 0;

void nonascii(char *what)
{
	fprintf(stderr, "look: %s contains non-ASCII characters.\n", what);
	exit(1);
}

int compare(char *prefix, char *word)
{
	char *p= prefix, *w= word;
	int cp, cw;

	do {
		do {
			if ((cp= *p++) == 0) return 0;
			if (!isascii(cp)) nonascii("prefix string");
		} while (dflg && !isspace(cp) && !isalnum(cp));

		if (dflg) {
			if (isspace(cp)) {
				while (isspace(*p)) p++;
				cp= ' ';
			}
		}
		if (fflg && isupper(cp)) cp= tolower(cp);

		do {
			if ((cw= *w++) == 0) return 1;
			if (!isascii(cw)) nonascii(wordlist);
		} while (dflg && !isspace(cw) && !isalnum(cw));

		if (dflg) {
			if (isspace(cw)) {
				while (isspace(*w)) w++;
				cw= ' ';
			}
		}
		if (fflg && isupper(cw)) cw= tolower(cw);
	} while (cp == cw);

	return cp - cw;
}

char *readword(FILE *f)
{
	static char word[MAXLEN + 2];
	int n;

	if (fgets(word, sizeof(word), f) == nil) {
		if (ferror(f)) {
			fprintf(stderr, "look: read error on %s",
				wordlist);
			exit(1);
		}
		return nil;
	}

	n= strlen(word);

	if (word[n-1] != '\n') {
		fprintf(stderr, "look: word from %s is too long\n", wordlist);
		exit(1);
	}
	word[n-1] = 0;

	return word;
}

void look(void)
{
	off_t low, mid, high;
	FILE *f;
	char *word;
	int c;

	if ((f= fopen(wordlist, "r")) == nil) {
		fprintf(stderr, "look: Can't open %s\n", wordlist);
		exit(1);
	}

	low= 0;

	fseek(f, (off_t) 0, 2);

	high= ftell(f);

	while (low <= high) {
		mid= (low + high) / 2;

		fseek(f, mid, 0);

		if (mid != 0) readword(f);

		if ((word= readword(f)) == nil)
			c= -1;
		else
			c= compare(string, word);

		if (c <= 0) high= mid - 1; else low= mid + 1;
	}
	fseek(f, low, 0);
	if (low != 0) readword(f);

	c=0;
	while (c >= 0 && (word= readword(f)) != nil) {
		c= compare(string, word);

		if (c == 0) puts(word);
	}
}

int main(int argc, char **argv)
{
	if (argc == 2) dflg= fflg= 1;

	while (argc > 1 && argv[1][0] == '-') {
		char *p= argv[1] + 1;

		while (*p != 0) {
			switch (*p++) {
			case 'd':	dflg= 1; break;
			case 'f':	fflg= 1; break;
			default:
				fprintf(stderr, "look: Bad flag: %c\n", p[-1]);
				exit(1);
			}
		}
		argc--;
		argv++;
	}
	if (argc == 3)
		wordlist= argv[2];
	else
	if (argc != 2) {
		fprintf(stderr, "Usage: look [-df] string [file]\n");
		exit(1);
	}
	string= argv[1];
	look();
	exit(0);
}
/* Kees J. Bot 24-5-89. */
