/*	sys/ioc_tty.h - Terminal ioctl() command codes.
 *							Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_TTY_H
#define _S_I_TTY_H

#include <minix/ioctl.h>

/* Terminal ioctls. */
#define TCGETS		_IOR('T',  8, struct termios) /* tcgetattr */
#define TCSETS		_IOW('T',  9, struct termios) /* tcsetattr, TCSANOW */
#define TCSETSW		_IOW('T', 10, struct termios) /* tcsetattr, TCSADRAIN */
#define TCSETSF		_IOW('T', 11, struct termios) /* tcsetattr, TCSAFLUSH */
#define TCSBRK		_IOW('T', 12, int)	      /* tcsendbreak */
#define TCDRAIN		_IO ('T', 13)		      /* tcdrain */
#define TCFLOW		_IOW('T', 14, int)	      /* tcflow */
#define TCFLSH		_IOW('T', 15, int)	      /* tcflush */
#define	TIOCGWINSZ	_IOR('T', 16, struct winsize)
#define	TIOCSWINSZ	_IOW('T', 17, struct winsize)
#define	TIOCGPGRP	_IOW('T', 18, int)
#define	TIOCSPGRP	_IOW('T', 19, int)
#define TIOCSFON	_IOW('T', 20, u8_t [8192])

/* Legacy <sgtty.h> */
#define TIOCGETP	_IOR('t',  1, struct sgttyb)
#define TIOCSETP	_IOW('t',  2, struct sgttyb)
#define TIOCGETC	_IOR('t',  3, struct tchars)
#define TIOCSETC	_IOW('t',  4, struct tchars)

/* Keyboard ioctls. */
#define KIOCSMAP	_IOW('k', 3, keymap_t)

#endif /* _S_I_TTY_H */
