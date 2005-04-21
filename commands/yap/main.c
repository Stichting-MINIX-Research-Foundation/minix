/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _MAIN_

# include "in_all.h"
# if USG_OPEN
# include <fcntl.h>
# endif
# if BSD4_2_OPEN
# include <sys/file.h>
# endif
# if POSIX_OPEN
# include <sys/types.h>
# include <fcntl.h>
# endif
# include "main.h"
# include "term.h"
# include "options.h"
# include "output.h"
# include "process.h"
# include "commands.h"
# include "display.h"
# include "prompt.h"

char	*strcpy();

STATIC int initialize();
# ifdef SIGTSTP
STATIC int suspsig();
# endif

int
main(argc,argv) register char ** argv; {

	register char ** av;

	if (! isatty(1)) {
		no_tty = 1;
	}
	argv[argc] = 0;
	progname = argv[0];
	if ((av = readoptions(argv)) == (char **) 0 ||
		initialize(*av ? 1 : 0)) {
		if (no_tty) {
			close(1);
			(VOID) dup(2);
		}
		putline("Usage: ");
		putline(argv[0]);
		putline(
" [-c] [-u] [-n] [-q] [-number] [+command] [file ... ]\n");
		flush();
		exit(1);
	}
	if (no_tty) {
		*--av = "cat";
		execve("/bin/cat", av, (char *) 0);
	}
	else	processfiles(argc-(av-argv), av);
	(VOID) quit();
	/* NOTREACHED */
}

char *mktemp();

/*
 * Open temporary file for reading and writing.
 * Panic if it fails
 */

static char indexfile[30], tempfile[30];

int
opentemp(i) {

	register fildes;
	register char *f;

	f = i ? mktemp(indexfile) : mktemp(tempfile);
# if BSD4_2_OPEN || USG_OPEN || POSIX_OPEN
	if ((fildes = open(f,O_RDWR|O_TRUNC|O_CREAT,0600)) < 0) {
# else
	if ((fildes = creat(f,0600)) <= 0 || close(fildes) < 0 ||
	    (fildes = open(f,2)) < 0) {
# endif
		panic("Couldn't open temporary file");
	}
	(VOID) unlink(f);
	return fildes;
}

/*
 * Collect initializing stuff here.
 */

STATIC int
initialize(x) {

	if (!(nopipe = x)) {
		/*
		 * Reading from pipe
		 */
		if (isatty(0)) {
			return 1;
		}
		stdf = dup(0);	/* Duplicate file descriptor of input */
		if (no_tty) return 0;
		/*
		 * Make sure standard input is from the terminal.
		 */
		(VOID) close(0);
# if BSD4_2_OPEN || USG_OPEN || POSIX_OPEN
		if (open("/dev/tty",O_RDONLY,0) != 0) {
# else
		if (open("/dev/tty",0) != 0) {
# endif
			putline("Couldn't open terminal\n");
			flush();
			exit(1);
		}
	}
	if (no_tty) return 0;
	(VOID) strcpy(tempfile,"/usr/tmp/yap_XXXXXX");
	(VOID) strcpy(indexfile,"/usr/tmp/yap-XXXXXX");
	/*
	 * Handle signals.
	 * Catch QUIT, DELETE and ^Z
	 */
	(VOID) signal(SIGQUIT,SIG_IGN);
	(VOID) signal(SIGINT, catchdel);
	ini_terminal();
# ifdef SIGTSTP
	if (signal(SIGTSTP,SIG_IGN) == SIG_DFL) {
		(VOID) signal(SIGTSTP,suspsig);
	}
# endif
	(VOID) signal(SIGQUIT,quit);
	return 0;
}

int
catchdel() {
	(VOID) signal(SIGINT, catchdel);
	interrupt = 1;
}

# ifdef SIGTSTP

/*
 * We had a SIGTSTP signal.
 * Suspend, by a call to this routine.
 */

VOID
suspend() {

	nflush();
	resettty();
	(VOID) signal(SIGTSTP,SIG_DFL);
#if BSD4_2_OPEN
	sigsetmask(sigblock(0)&~(1 << (SIGTSTP - 1)));
#endif
	(VOID) kill(0, SIGTSTP);
	/*
	 * We are not here anymore ...
	 *

	 *
	 * But we arive here ...
	 */
	inittty();
	putline(TI);
	flush();
	(VOID) signal(SIGTSTP,suspsig);
}

/*
 * SIGTSTP signal handler.
 * Just indicate that we had one, ignore further ones and return.
 */

STATIC int
suspsig() {

	suspend();
	if (DoneSetJmp) longjmp(SetJmpBuf, 1);
}
# endif

/*
 * quit : called on exit.
 * I bet you guessed that much.
 */

int
quit() {

	clrbline();
	resettty();
	flush();
	exit(0);
}

/*
 * Exit, but nonvoluntarily.
 * At least tell the user why.
 */

VOID
panic(s) char *s; {

	putline("\007\007\007\r\n");
	putline(s);
	putline("\r\n");
	quit();
}
