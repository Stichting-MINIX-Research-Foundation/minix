/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _PROCESS_

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
# include <sys/types.h>
# include <sys/stat.h>
# include "process.h"
# include "commands.h"
# include "display.h"
# include "prompt.h"
# include "getline.h"
# include "main.h"
# include "options.h"
# include "output.h"

static int nfiles;		/* Number of filenames on command line */

/*
 * Visit a file, file name is "fn".
 */

VOID
visitfile(fn) char *fn; {
	struct stat statbuf;

	if (stdf > 0) {
		/*
		 * Close old input file
		 */
		(VOID) close(stdf);
	}
	currentfile = fn;
# if USG_OPEN || BSD4_2_OPEN || POSIX_OPEN
	if ((stdf = open(fn,O_RDONLY,0)) < 0) {
# else
	if ((stdf = open(fn,0)) < 0) {
# endif
		error(": could not open");
		maxpos = 0;
	}
	else {	/* Get size for percentage in prompt */
		(VOID) fstat(stdf, &statbuf);
		maxpos = statbuf.st_size;
	}
	do_clean();
	d_clean();
}

/*
 * process the input files, one by one.
 * If there is none, input is from a pipe.
 */

VOID
processfiles(n,argv) char ** argv; {

	static char *dummies[3];
	long arg;

	if (!(nfiles = n)) {
		/*
		 * Input from pipe
		 */
		currentfile = "standard-input";
		/*
		 * Take care that *(filenames - 1) and *(filenames + 1) are 0
		 */
		filenames = &dummies[1];
		d_clean();
		do_clean();
	}
	else {
		filenames = argv;
		(VOID) nextfile(0);
	}
	*--argv = 0;
	if (startcomm) {
		n = getcomm(&arg);
		if (commands[n].c_flags & NEEDS_SCREEN) {
			redraw(0);
		}
		do_comm(n,arg);
		startcomm = 0;
	}
	redraw(1);
	if (setjmp(SetJmpBuf)) {
		nflush();
		redraw(1);
	}
	DoneSetJmp = 1;
	for (;;) {
		interrupt = 0;
		n = getcomm(&arg);
		do_comm(n, arg);
	}
}

/*
 * Get the next file the user asks for.
 */

int
nextfile(n) {
	register i;

	if ((i = filecount + n) >= nfiles || i < 0) {
		return 1;
	}
	filecount = i;
	visitfile(filenames[i]);
	return 0;
}
