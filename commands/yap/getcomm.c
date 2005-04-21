/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/*
 * Command reader, also executes shell escapes
 */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _GETCOMM_

# include <ctype.h>
# include "in_all.h"
# include "term.h"
# include "process.h"
# include "getcomm.h"
# include "commands.h"
# include "prompt.h"
# include "main.h"
# include "output.h"
# include "getline.h"
# include "machine.h"
# include "keys.h"
# include "display.h"
# include "assert.h"

#if USG_OPEN
#include <fcntl.h>
#endif
#if POSIX_OPEN
#include <sys/types.h>
#include <fcntl.h>
#endif

char	*strcpy(),
	*getenv();

STATIC int	killchar();

/*
 * Read a line from the terminal, doing line editing.
 * The parameter s contains the prompt for the line.
 */

char *
readline(s) char *s; {

	static char buf[80];
	register char *p = buf;
	register int ch;
	register int pos;

	clrbline();
	putline(s);
	pos = strlen(s);
	while ((ch = getch()) != '\n' && ch != '\r') {
		if (ch == -1) {
			/*
			 * Can only occur because of an interrupted read.
			 */
			ch = erasech;
			interrupt = 0;
		}
		if (ch == erasech) {
			/*
			 * Erase last char
			 */
			if (p == buf) {
				/*
				 * There was none, so return
				 */
				return (char *) 0;
			}
			pos -= killchar(*--p);
			if (*p != '\\') continue;
		}
		if (ch == killch) {
			/*
			 * Erase the whole line
			 */
			if (!(p > buf && *(p-1) == '\\')) {
				while (p > buf) {
					pos -= killchar(*--p);
				}
				continue;
			}
			pos -= killchar(*--p);
		}
		if (p > &buf[78] || pos >= COLS - 2) {
			/*
			 * Line does not fit.
			 * Simply refuse to make it any longer
			 */
			pos -= killchar(*--p);
		}
		*p++ = ch;
		if (ch < ' ' || ch >= 0177) {
			fputch('^');
			pos++;
			ch ^= 0100;
		}
		fputch(ch);
		pos++;
	}
	fputch('\r');
	*p++ = '\0';
	flush();
	return buf;
}

/*
 * Erase a character from the command line.
 */

STATIC int
killchar(c) {

	backspace();
	putch(' ');
	backspace();
	if (c < ' ' || c >= 0177) {
		(VOID) killchar(' ');
		return 2;
	}
	return 1;
}

/*
 * Do a shell escape, after expanding '%' and '!'.
 */

VOID
shellescape(p, esc_char) register char *p; {

	register char *p2;	/* walks through command */
	register int id;	/* procid of child */
	register int cnt;	/* prevent array bound errors */
	register int lastc = 0;	/* will contain the previous char */
# ifdef SIGTSTP
	VOID (*savetstp)();
# endif
	static char previous[256];	/* previous command */
	char comm[256];			/* space for command */
	int piped[2];

	p2 = comm;
	*p2++ = esc_char;
	cnt = 253;
	while (*p) {
		/*
		 * expand command
		 */
		switch(*p++) {
		  case '!':
			/*
			 * An unescaped ! expands to the previous
			 * command, but disappears if there is none
			 */
			if (lastc != '\\') {
				if (*previous) {
					id = strlen(previous);
					if ((cnt -= id) <= 0) break;
					(VOID) strcpy(p2,previous);
					p2 += id;
				}
			}
			else {
				*(p2-1) = '!';
			}
			continue;
		  case '%':
			/*
			 * An unescaped % will expand to the current
			 * filename, but disappears is there is none
			 */
			if (lastc != '\\') {
				if (nopipe) {
					id = strlen(currentfile);
					if ((cnt -= id) <= 0) break;
					(VOID) strcpy(p2,currentfile);
					p2 += id;
				}
			}
			else {
				*(p2-1) = '%';
			}
			continue;
		  default:
			lastc = *(p-1);
			if (cnt-- <= 0) break;
			*p2++ = lastc;
			continue;
		}
		break;
	}
	clrbline();
	*p2 = '\0';
	if (!stupid) {
		/*
		 * Display expanded command
		 */
		cputline(comm);
		putline("\r\n");
	}
	flush();
	(VOID) strcpy(previous,comm + 1);
	resettty();
	if (esc_char == '|' && pipe(piped) < 0) {
		error("Cannot create pipe");
		return;
	}
	if ((id = fork()) < 0) {
		error("Cannot fork");
		return;
	}
	if (id == 0) {
		/*
		 * Close files, as child might need the file descriptors
		 */
		cls_files();
		if (esc_char == '|') {
			close(piped[1]);
#if USG_OPEN || POSIX_OPEN
			close(0);
			fcntl(piped[0], F_DUPFD, 0);
#else
			dup2(piped[0], 0);
#endif
			close(piped[0]);
		}
		execl("/bin/sh", "sh", "-c", comm + 1, (char *) 0);
		exit(1);
	}
	(VOID) signal(SIGINT,SIG_IGN);
	(VOID) signal(SIGQUIT,SIG_IGN);
# ifdef SIGTSTP
	if ((savetstp = signal(SIGTSTP,SIG_IGN)) != SIG_IGN) {
		(VOID) signal(SIGTSTP,SIG_DFL);
	}
# endif
	if (esc_char == '|') {
		(VOID) close(piped[0]);
		(VOID) signal(SIGPIPE, SIG_IGN);
		wrt_fd(piped[1]);
		(VOID) close(piped[1]);
	}
	while ((lastc = wait((int *) 0)) != id && lastc >= 0)  {
		/*
		 * Wait for child, making sure it is the one we expected ...
		 */
	}
	(VOID) signal(SIGINT,catchdel);
	(VOID) signal(SIGQUIT,quit);
# ifdef SIGTSTP
	(VOID) signal(SIGTSTP, savetstp);
# endif
	inittty();
}

/*
 * Get all those commands ...
 */

int
getcomm (plong) long   *plong; {
	int	c;
	long	count;
	char	*p;
	int	i;
	int	j;
	char	buf[10];

	for (;;) {
		count = 0;
		give_prompt();
		while (isdigit((c = getch()))) {
			count = count * 10 + (c - '0');
		}
		*plong = count;
		p = buf;
		for (;;) {
			if (c == -1) {
				/*
				 * This should never happen, but it does,
				 * when the user gives a TSTP signal (^Z) or
				 * an interrupt while the program is trying
				 * to read a character from the terminal.
				 * In this case, the read is interrupted, so
				 * we end up here.
				 * Right, we will have to read again.
				 */
				if (interrupt) return 1;
				break;
			}
			*p++ = c;
			*p = 0;
			if ((i = match(buf, &j, currmap->k_mach)) > 0) {
				/*
				 * The key sequence matched. We have a command
				 */
				return j;
			}
			if (i == 0) return 0;
			/*
			 * We have a prefix of a command.
			 */
			assert(i == FSM_ISPREFIX);
			c = getch();
		}
	}
	/* NOTREACHED */
}
