/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/* $Header$ */

# ifndef _COMMANDS_
# define PUBLIC extern
# else
# define PUBLIC
# endif

/* Flags, describing properties of commands */

# define SCREENSIZE_ADAPT	01	/* Can change screen size */
# define SCROLLSIZE_ADAPT	02	/* Can change scrollsize */
# define TONEXTFILE		04	/* to next file */
# define AHEAD			010	/* goes ahead in the file */
# define BACK			020	/* goes back in the file */
# define NEEDS_SCREEN		040	/* needs screen info */
# define TOPREVFILE		0100	/* to previous file */
# define STICKY			0200	/* remember for lastcomm */
# define NEEDS_COUNT		0400	/* command needs a count */
# define ESC			01000	/* shell or pipe escape */

extern struct commands {
    char *c_cmd;		/* Short mnemonic for a command */
    int c_flags;		/* describes command properties */
    int (*c_func)();		/* Function executing the command */
    char *c_descr;		/* Short command description */
} commands[];

int	do_chkm();
/*
 * int	do_chkm(cnt)
 * long cnt;			Ignored
 *
 * Switch to other keymap
 */

VOID	do_comm();
/*
 * void do_comm(command, count)
 * int command;			Index in the commands array
 * long count;			Some commands possibly take a count
 *
 * Checks and executes commands.
 */

int	lookup();
/*
 * int lookup(str)
 * char *str;
 *
 * Searches the string "str" in the command list.
 * It returns its index if it exists, otherwise it returns 0.
 */

VOID	wrt_fd();
/*
 * void wrt_fd(fd)
 * int fd;			File descriptor
 *
 * Write input to file descriptor fd
 */
# undef PUBLIC
