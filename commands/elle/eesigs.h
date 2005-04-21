/* ELLE - Copyright 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/* EESIGS.H
 *	This file is only provided for inclusion only by V6 systems, where
 * the standard /usr/include/signal.h file may not exist and thus we
 * need to do our own definitions.
 */

/* Signals marked with "*" cause a core image dump
 * if not caught or ignored. */

#define	SIGHUP	1	/*   Hangup (eg dialup carrier lost) */
#define	SIGINT	2	/*   Interrupt (user TTY interrupt) */
#define	SIGQUIT	3	/* * Quit (user TTY interrupt) */
#define	SIGILL	4	/* * Illegal Instruction (not reset when caught) */
#define	SIGTRAP	5	/* * Trace Trap (not reset when caught) */
#define	SIGIOT	6	/* * IOT instruction */
#define	SIGEMT	7	/* * EMT instruction */
#define	SIGFPE	8	/* * Floating Point Exception */
#define	SIGKILL	9	/*   Kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* * Bus Error */
#define	SIGSEGV	11	/* * Segmentation Violation */
#define	SIGSYS	12	/* * Bad argument to system call */
#define	SIGPIPE	13	/*   Write on a pipe with no one to read it */
#define	SIGALRM	14	/*   Alarm Clock */
#define	SIGTERM	15	/*   Software termination signal (from "kill" pgm) */

#define	SIG_DFL	(int (*)())0	/* Arg to "signal" to resume default action */
#define	SIG_IGN	(int (*)())1	/* Arg to "signal" to ignore this sig */
