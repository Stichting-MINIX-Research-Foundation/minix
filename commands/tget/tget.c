/*	tget 1.0 - get termcap values			Author: Kees J. Bot
 *								6 Mar 1994
 */
#define nil 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>

void fputchar(int c)
{
	putchar(c);
}

void usage(void)
{
	fprintf(stderr,
"Usage: tget [-flag id] [-num id] [-str id] [-goto col line] [[-echo] string]\n"
		);
	exit(-1);
}

void main(int argc, char **argv)
{
	char termbuf[1024];
	char string[256], *pstr;
	char *term;
	int i;
	int excode= 0;

	if ((term= getenv("TERM")) == nil) {
		fprintf(stderr, "tget: $TERM is not set\n");
		exit(-1);
	}

	if (tgetent(termbuf, term) != 1) {
		fprintf(stderr, "tget: no termcap entry for '%s'\n", term);
		exit(-1);
	}

	for (i= 1; i < argc; i++) {
		char *option= argv[i];
		char *id;

		if (option[0] != '-') {
			fputs(option, stdout);
			continue;
		}

		if (++i == argc) usage();
		id= argv[i];

		if (strcmp(option, "-flag") == 0) {
			excode= tgetflag(id) ? 0 : 1;
		} else
		if (strcmp(option, "-num") == 0) {
			int num;

			if ((num= tgetnum(id)) == -1) {
				excode= 1;
			} else {
				excode= 0;
				printf("%d", num);
			}
		} else
		if (strcmp(option, "-str") == 0) {
			char *str;

			if ((str= tgetstr(id, (pstr= string, &pstr))) == nil) {
				excode= 1;
			} else {
				excode= 0;
				tputs(str, 0, fputchar);
			}
		} else
		if (strcmp(option, "-goto") == 0) {
			char *cm;
			int col, line;

			col= atoi(id);
			if (++i == argc) usage();
			line= atoi(argv[i]);

			if ((cm= tgetstr("cm", (pstr= string, &pstr))) == nil) {
				excode= 1;
			} else {
				excode= 0;
				tputs(tgoto(cm, col, line), 0, fputchar);
			}
		} else
		if (strcmp(option, "-echo") == 0) {
			fputs(id, stdout);
		} else {
			usage();
		}
	}
	exit(excode);
}
