/* The <sgtty.h> header contains data structures for ioctl(). */

#ifndef _SGTTY_H
#define _SGTTY_H

/* Should not be used, nor extended. Termios.h is the replacement for
 * sgtty.h for tty functions, and ioctl replaced code should be moved to
 * sys/ioctl.h and specific header files in the sys, or minix directory.
 */
#include <sys/ioctl.h>		/* Ouch. */

struct sgttyb {
  char sg_ispeed;		/* input speed */
  char sg_ospeed;		/* output speed */
  char sg_erase;		/* erase character */
  char sg_kill;			/* kill character */
  int  sg_flags;		/* mode flags */
};

struct tchars {
  char t_intrc;			/* SIGINT char */
  char t_quitc;			/* SIGQUIT char */
  char t_startc;		/* start output (initially CTRL-Q) */
  char t_stopc;			/* stop output	(initially CTRL-S) */
  char t_eofc;			/* EOF (initially CTRL-D) */
  char t_brkc;			/* input delimiter (like nl) */
};

#if !_SYSTEM			/* the kernel doesn't want to see the rest */

/* Field names */
#define XTABS	     0006000	/* do tab expansion */
#define BITS8        0001400	/* 8 bits/char */
#define BITS7        0001000	/* 7 bits/char */
#define BITS6        0000400	/* 6 bits/char */
#define BITS5        0000000	/* 5 bits/char */
#define EVENP        0000200	/* even parity */
#define ODDP         0000100	/* odd parity */
#define RAW	     0000040	/* enable raw mode */
#define CRMOD	     0000020	/* map lf to cr + lf */
#define ECHO	     0000010	/* echo input */
#define CBREAK	     0000002	/* enable cbreak mode */
#define COOKED       0000000	/* neither CBREAK nor RAW */

#define DCD          0100000	/* Data Carrier Detect */

/* Line speeds */
#define B0		   0	/* code for line-hangup */
#define B110		   1
#define B300		   3
#define B1200		  12
#define B2400		  24
#define B4800		  48
#define B9600 		  96
#define B19200		 192
#define B38400		 195
#define B57600		 194
#define B115200		 193

/* Things Minix supports but not properly */
/* the divide-by-100 encoding ain't too hot */
#define ANYP         0000300
#define B50                0
#define B75                0
#define B134               0
#define B150               0
#define B200               2
#define B600               6
#define B1800             18
#define B3600             36
#define B7200             72
#define EXTA             192
#define EXTB               0

/* Things Minix doesn't support but are fairly harmless if used */
#define NLDELAY      0001400
#define TBDELAY      0006000
#define CRDELAY      0030000
#define VTDELAY      0040000
#define BSDELAY      0100000
#define ALLDELAY     0177400

/* Copied from termios.h: */
struct winsize
{
	unsigned short	ws_row;		/* rows, in characters */
	unsigned short	ws_col;		/* columns, in characters */
	unsigned short	ws_xpixel;	/* horizontal size, pixels */
	unsigned short	ws_ypixel;	/* vertical size, pixels */
};
#endif /* !_SYSTEM */
#endif /* _SGTTY_H */
