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
#define TIOCSFON_OLD	_IOW('T', 20, u8_t [8192])
#define TIOCSFON	_IOW_BIG(1, u8_t [8192])

/* Keyboard ioctls. */
#define KIOCBELL        _IOW('k', 1, struct kio_bell)
#define KIOCSLEDS       _IOW('k', 2, struct kio_leds)
#define KIOCSMAP	_IOW('k', 3, keymap_t)

/* /dev/video ioctls. */
#define TIOCMAPMEM	_IORW('v', 1, struct mapreqvm)
#define TIOCUNMAPMEM	_IORW('v', 2, struct mapreqvm)

#endif /* _S_I_TTY_H */
