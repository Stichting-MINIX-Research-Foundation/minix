/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _OUTPUT_
# define PUBLIC extern
# else
# define PUBLIC
# endif

PUBLIC int _ocnt;
PUBLIC char *_optr;

#define putch(ch)	if (1) {if (--_ocnt <= 0) flush(); *_optr++ = (ch);} else

VOID	flush();
/*
 * void flush()
 *
 * Write the output buffer to the screen
 */

VOID	nflush();
/*
 * void nflush()
 *
 * Clear output buffer, but do not write it
 */

int	fputch();
/*
 * int fputch(c)
 * int c;		The character to be printed
 *
 * Put character "c" in output buffer and flush if necessary.
 */

VOID	putline();
/*
 * void putline(s)
 * char *s;		The string to be printed
 *
 * Put string "s" in output buffer  etc...
 */

VOID	cputline();
/*
 * void cputline(s)
 * char *s;		The string to be handled
 *
 * Put string "s" in the output buffer, expanding control characters
 */

VOID	prnum();
/*
 * void prnum(n)
 * long n;		The number to be printed
 *
 * print the number "n", using putch.
 */

char	*getnum();
/*
 * char *getnum(n)
 * long n;		The number to be converted to a string
 *
 * Convert a number to a string and return a pointer to it.
 */
# undef PUBLIC
