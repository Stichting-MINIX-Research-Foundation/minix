/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _PROCESS_
# define PUBLIC extern
# else
# define PUBLIC
# endif

# include <setjmp.h>

PUBLIC	jmp_buf	SetJmpBuf;
PUBLIC	int DoneSetJmp;

PUBLIC int	stdf;		/* input file descriptor */
PUBLIC int	filecount;	/* index in filename table */
PUBLIC char **	filenames;	/* the filenametable */
PUBLIC char *	currentfile;	/* Name of current file */
PUBLIC long	maxpos;		/* Size of file */

VOID	visitfile();
/*
 * void visitfile(fn)
 * char *fn;			name of file to be visited
 *
 * Opens the file "fn" and gives an error message if this fails.
 */

VOID	processfiles();
/*
 * void processfiles(n,argv)
 * int n;			number of files to be handled
 * char ** argv;		names of the files
 *
 * Does all the work according to the divide and conquer method
 */

int	nextfile();
/*
 * int nextfile(n)
 * int n;
 *
 * Visits n'th next file. If not there in argument list, return 1.
 * Otherwise return 0.
 */

# undef PUBLIC
