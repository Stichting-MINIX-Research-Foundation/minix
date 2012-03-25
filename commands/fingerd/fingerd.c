/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)in.fingerd.c 1.1 87/12/21 SMI"; /* from UCB 5.1 6/6/85 */
#endif /* not lint */

/*
 * Finger server.
 */
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main( int argc, char *argv[] );
void fatal( char *prog, char *s );

int main(argc, argv)
	char *argv[];
{
	register char *sp;
	char line[512];
	int i, p[2], pid, status;
	FILE *fp;
	char *av[4];

	line[0] = '\0';
	fgets(line, sizeof(line), stdin);
	sp = line + strlen(line);
	if (sp > line && *--sp == '\n') *sp = '\0';
	sp = line;
	av[0] = "finger";
	i = 1;
	while (1) {
		while (isspace(*sp))
			sp++;
		if (!*sp)
			break;
		if (*sp == '/' && (sp[1] == 'W' || sp[1] == 'w')) {
			sp += 2;
			av[i++] = "-l";
		}
		if (*sp && !isspace(*sp)) {
			av[i++] = sp;
			while (*sp && !isspace(*sp))
				sp++;
			*sp = '\0';
		}
	}
	av[i] = 0;
	if (pipe(p) < 0)
		fatal(argv[0], "pipe");
	if ((pid = fork()) == 0) {
		close(p[0]);
		if (p[1] != 1) {
			dup2(p[1], 1);
			close(p[1]);
		}
		execv("/usr/bin/finger", av);
		printf("No finger program found\n");
		fflush(stdout);
		_exit(1);
	}
	if (pid == -1)
		fatal(argv[0], "fork");
	close(p[1]);
	if ((fp = fdopen(p[0], "r")) == NULL)
		fatal(argv[0], "fdopen");
	while ((i = getc(fp)) != EOF) {
		if (i == '\n')
			putchar('\r');
		putchar(i);
	}
	fclose(fp);
	while ((i = wait(&status)) != pid && i != -1)
		;
	return(0);
}

void fatal(prog, s)
	char *prog, *s;
{

	fprintf(stderr, "%s: ", prog);
	perror(s);
	exit(1);
}
