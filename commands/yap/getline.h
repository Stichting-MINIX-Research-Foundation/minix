/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _GETLINE_
# define PUBLIC extern
# else
# define PUBLIC
# endif

char *	getline();
/*
 * char * getline(ln,disable_interrupt)
 * long ln;			The line number of the line to be returned
 * int disable_interrupt;	1 if interrupts must be ignored, 0 otherwise
 *
 * Returns a pointer to the line with linenumber "ln".
 * It returns 0 if
 * - there was an interrupt, and interrupts were not disabled, or
 * - there is no line with linenumber "ln".
 */

char *	alloc();
/*
 * char * alloc(size, isblock)
 * unsigned size;		The size in bytes
 * int isblock;			Flag indicating whether this is a file-text
 *				block
 *
 * Return a pointer to a block of "size" bytes.
 * Panics if no core can be found.
 */

VOID	do_clean();
/*
 * void do_clean()
 *
 * Cleans up and initializes.
 */

VOID	cls_files();
/*
 * void cls_files()
 *
 * Closes files. Useful for shell escapes.
 */

int	getch();
/*
 * int getch()
 *
 * Get a character from input or command option line (only at start up).
 * Some systems allow us to do some workahead while the user is
 * thinking/reading. Use this to get parts of the input file in core.
 */

long	to_lastline();
/*
 * long to_lastline()
 *
 * Finds the last line of the file, and returns its number.
 * This command can be interrupted, in which case it returns 0.
 */

long	getpos();
/*
 * long getpos(line);
 *
 * get offset of line "line" in the input
 */
# undef PUBLIC
