/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _MAIN_
# define PUBLIC extern
# else
# define PUBLIC
# endif

PUBLIC int	nopipe;		/* Not reading from pipe? */
PUBLIC char *	progname;	/* Name of this program */
PUBLIC int	interrupt;	/* Interrupt given? */
PUBLIC int	no_tty;		/* output not to a terminal, behave like cat */

int	main();
/*
 * int main(argc,argv)
 * int argc;		Argument count
 * char *argv[];	The arguments
 *
 * Main program.
 */

int	opentemp();
/*
 * int opentemp(i)
 * int i;		Either 0 or 1, indicates which temporary to open
 *
 * Returns a file descriptor for the temporary file, or panics if
 * it couldn't open one.
 */

int	catchdel();
/*
 * int catchdel();
 *
 * interrupt handler. Does not return a value, but PCC has some
 * difficulty with the type pointer to function returning void.
 * This routine only sets a flag indicating that there was an interrupt.
 */

int	quit();
/*
 * int quit();
 *
 * Quit signal handler. Also used for normal exits.
 * It resets the terminal and exits
 */

VOID	panic();
/*
 * void panic(str)
 * char *str;		Reason for panic
 *
 * Panic, but at least tell the user why.
 */

# ifdef SIGTSTP
VOID	suspend();
/*
 * void suspend()
 *
 * Suspends this process
 */
# endif

# undef PUBLIC
