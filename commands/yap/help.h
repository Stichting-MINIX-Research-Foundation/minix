/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _HELP_
# define PUBLIC extern
# else
# define PUBLIC
# endif

int	do_help();
/*
 * int do_help(cnt);
 * long cnt;			This is ignored, but a count is given
 *				to any command
 *
 * Give a summary of commands
 */

# undef PUBLIC
