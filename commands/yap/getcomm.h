/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _GETCOMM_
# define PUBLIC extern
# else
# define PUBLIC
# endif

int	getcomm();
/*
 * int getcomm()
 *
 * Reads commands given by the user. The command is returned.
 */

VOID	shellescape();
/*
 * void shellescape(command)
 * char *command;		The shell command to be executed
 *
 * Expands '%' and '!' in the command "command" to the current filename
 * and the previous command respectively, and then executes "command".
 */

char *	readline();
/*
 * char * readline(prompt)
 * char *prompt;		Prompt given to the user
 *
 * Gives a prompt "prompt" and reads a line to be typed in by the user.
 * A pointer to this line is returned. Space for this line is static.
 */

# undef PUBLIC
