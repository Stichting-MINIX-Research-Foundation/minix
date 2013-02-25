/* The <termios.h> header is used for controlling tty modes. */

#ifndef _TERMIOS_H
#define _TERMIOS_H

typedef unsigned short tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS		   20	/* size of cc_c array, some extra space
				 * for extensions. */

/* Primary terminal control structure. POSIX Table 7-1. */
struct termios {
  tcflag_t c_iflag;		/* input modes */
  tcflag_t c_oflag;		/* output modes */
  tcflag_t c_cflag;		/* control modes */
  tcflag_t c_lflag;		/* local modes */
  speed_t  c_ispeed;		/* input speed */
  speed_t  c_ospeed;		/* output speed */
  cc_t c_cc[NCCS];		/* control characters */
};

/* Values for termios c_iflag bit map.  POSIX Table 7-2. */
#define BRKINT		0x0001	/* signal interrupt on break */
#define ICRNL		0x0002	/* map CR to NL on input */
#define IGNBRK		0x0004	/* ignore break */
#define IGNCR		0x0008	/* ignore CR */
#define IGNPAR		0x0010	/* ignore characters with parity errors */
#define INLCR		0x0020	/* map NL to CR on input */
#define INPCK		0x0040	/* enable input parity check */
#define ISTRIP		0x0080	/* mask off 8th bit */
#define IXOFF		0x0100	/* enable start/stop input control */
#define IXON		0x0200	/* enable start/stop output control */
#define PARMRK		0x0400	/* mark parity errors in the input queue */

/* Values for termios c_oflag bit map.  POSIX Sec. 7.1.2.3. */
#define OPOST		0x0001	/* perform output processing */

/* Values for termios c_cflag bit map.  POSIX Table 7-3. */
#define CLOCAL		0x0001	/* ignore modem status lines */
#define CREAD		0x0002	/* enable receiver */
#define CSIZE		0x000C	/* number of bits per character */
#define		CS5	0x0000	/* if CSIZE is CS5, characters are 5 bits */
#define		CS6	0x0004	/* if CSIZE is CS6, characters are 6 bits */
#define		CS7	0x0008	/* if CSIZE is CS7, characters are 7 bits */
#define		CS8	0x000C	/* if CSIZE is CS8, characters are 8 bits */
#define CSTOPB		0x0010	/* send 2 stop bits if set, else 1 */
#define HUPCL		0x0020	/* hang up on last close */
#define PARENB		0x0040	/* enable parity on output */
#define PARODD		0x0080	/* use odd parity if set, else even */

/* Values for termios c_lflag bit map.  POSIX Table 7-4. */
#define ECHO		0x0001	/* enable echoing of input characters */
#define ECHOE		0x0002	/* echo ERASE as backspace */
#define ECHOK		0x0004	/* echo KILL */
#define ECHONL		0x0008	/* echo NL */
#define ICANON		0x0010	/* canonical input (erase and kill enabled) */
#define IEXTEN		0x0020	/* enable extended functions */
#define ISIG		0x0040	/* enable signals */
#define NOFLSH		0x0080	/* disable flush after interrupt or quit */
#define TOSTOP		0x0100	/* send SIGTTOU (job control, not implemented*/

/* Indices into c_cc array.  Default values in parentheses. POSIX Table 7-5. */
#define VEOF                 0	/* cc_c[VEOF] = EOF char (^D) */
#define VEOL                 1	/* cc_c[VEOL] = EOL char (undef) */
#define VERASE               2	/* cc_c[VERASE] = ERASE char (^H) */
#define VINTR                3	/* cc_c[VINTR] = INTR char (DEL) */
#define VKILL                4	/* cc_c[VKILL] = KILL char (^U) */
#define VMIN                 5	/* cc_c[VMIN] = MIN value for timer */
#define VQUIT                6	/* cc_c[VQUIT] = QUIT char (^\) */
#define VTIME                7	/* cc_c[VTIME] = TIME value for timer */
#define VSUSP                8	/* cc_c[VSUSP] = SUSP (^Z, ignored) */
#define VSTART               9	/* cc_c[VSTART] = START char (^S) */
#define VSTOP               10	/* cc_c[VSTOP] = STOP char (^Q) */

/* Values for the baud rate settings.  POSIX Table 7-6. */
#define B0		0x0000	/* hang up the line */
#define B50		0x1000	/* 50 baud */
#define B75		0x2000	/* 75 baud */
#define B110		0x3000	/* 110 baud */
#define B134		0x4000	/* 134.5 baud */
#define B150		0x5000	/* 150 baud */
#define B200		0x6000	/* 200 baud */
#define B300		0x7000	/* 300 baud */
#define B600		0x8000	/* 600 baud */
#define B1200		0x9000	/* 1200 baud */
#define B1800		0xA000	/* 1800 baud */
#define B2400		0xB000	/* 2400 baud */
#define B4800		0xC000	/* 4800 baud */
#define B9600		0xD000	/* 9600 baud */
#define B19200		0xE000	/* 19200 baud */
#define B38400		0xF000	/* 38400 baud */

/* Optional actions for tcsetattr().  POSIX Sec. 7.2.1.2. */
#define TCSANOW            1	/* changes take effect immediately */
#define TCSADRAIN          2	/* changes take effect after output is done */
#define TCSAFLUSH          3	/* wait for output to finish and flush input */

/* Queue_selector values for tcflush().  POSIX Sec. 7.2.2.2. */
#define TCIFLUSH           1	/* flush accumulated input data */
#define TCOFLUSH           2	/* flush accumulated output data */
#define TCIOFLUSH          3	/* flush accumulated input and output data */

/* Action values for tcflow().  POSIX Sec. 7.2.2.2. */
#define TCOOFF             1	/* suspend output */
#define TCOON              2	/* restart suspended output */
#define TCIOFF             3	/* transmit a STOP character on the line */
#define TCION              4	/* transmit a START character on the line */

int tcsendbreak(int _fildes, int _duration);
int tcdrain(int _filedes);
int tcflush(int _filedes, int _queue_selector);
int tcflow(int _filedes, int _action);
speed_t cfgetispeed(const struct termios *_termios_p);
speed_t cfgetospeed(const struct termios *_termios_p);
int cfsetispeed(struct termios *_termios_p, speed_t _speed);
int cfsetospeed(struct termios *_termios_p, speed_t _speed);
int tcgetattr(int _filedes, struct termios *_termios_p);
int tcsetattr(int _filedes, int _opt_actions, const struct termios
	*_termios_p);

#ifndef cfgetispeed
#define cfgetispeed(termios_p)		((termios_p)->c_ispeed)
#endif
#ifndef cfgetospeed
#define cfgetospeed(termios_p)		((termios_p)->c_ospeed)
#endif
#ifndef cfsetispeed
#define cfsetispeed(termios_p, speed)	((termios_p)->c_ispeed = (speed), 0)
#endif
#ifndef cfsetospeed
#define cfsetospeed(termios_p, speed)	((termios_p)->c_ospeed = (speed), 0)
#endif

/* Here are the local extensions to the POSIX standard for Minix. */

/* Extensions to the termios c_iflag bit map.  */
#define IXANY		0x0800	/* allow any key to continue ouptut */
#define SCANCODES	0x1000	/* send scancodes */

/* Extensions to the termios c_oflag bit map. They are only active iff
 * OPOST is enabled. */
#define ONLCR		0x0002	/* Map NL to CR-NL on output */
#define XTABS		0x0004	/* Expand tabs to spaces */
#define ONOEOT		0x0008	/* discard EOT's (^D) on output) */

/* Extensions to the termios c_lflag bit map.  */
#define LFLUSHO		0x0200	/* Flush output. */

/* Extensions to the c_cc array. */
#define VREPRINT	  11	/* cc_c[VREPRINT] (^R) */
#define VLNEXT            12    /* cc_c[VLNEXT] (^V) */
#define VDISCARD          13    /* cc_c[VDISCARD] (^O) */

/* Extensions to baud rate settings. */
#if defined(__minix) && defined(_NETBSD_SOURCE)
#define B57600		0x0100	/* 57600 baud */
#define B115200		0x0200	/* 115200 baud */
#define B230400		0x0400	/* 230400 baud */
#define B460800		0x0800	/* 460800 baud */
#define B921600		0x1000	/* 921600 baud */
#define B1843200	0x2000	/* 1843200 baud */
#define B3000000	0x4000	/* 3000000 baud */
#define B3686400	0x8000	/* 3686400 baud */
#endif /* defined(__minix) && defined(_NETBSD_SOURCE) */

/* These are the default settings used by the kernel and by 'stty sane' */

#define TCTRL_DEF	(CREAD | CS8 | HUPCL)
#define TINPUT_DEF	(BRKINT | ICRNL | IXON | IXANY)
#define TOUTPUT_DEF	(OPOST | ONLCR)
#define TLOCAL_DEF	(ISIG | IEXTEN | ICANON | ECHO | ECHOE)
#define TSPEED_DEF	B9600

#define TEOF_DEF	'\4'	/* ^D */
#define TEOL_DEF	_POSIX_VDISABLE
#define TERASE_DEF	'\10'	/* ^H */
#define TINTR_DEF	'\3'	/* ^C */
#define TKILL_DEF	'\25'	/* ^U */
#define TMIN_DEF	1
#define TQUIT_DEF	'\34'	/* ^\ */
#define TSTART_DEF	'\21'	/* ^Q */
#define TSTOP_DEF	'\23'	/* ^S */
#define TSUSP_DEF	'\32'	/* ^Z */
#define TTIME_DEF	0
#define	TREPRINT_DEF	'\22'	/* ^R */
#define	TLNEXT_DEF	'\26'	/* ^V */
#define	TDISCARD_DEF	'\17'	/* ^O */

/* Window size. This information is stored in the TTY driver but not used.
 * This can be used for screen based applications in a window environment. 
 * The ioctls TIOCGWINSZ and TIOCSWINSZ can be used to get and set this 
 * information.
 */

struct winsize
{
	unsigned short	ws_row;		/* rows, in characters */
	unsigned short	ws_col;		/* columns, in characters */
	unsigned short	ws_xpixel;	/* horizontal size, pixels */
	unsigned short	ws_ypixel;	/* vertical size, pixels */
};

#endif /* _TERMIOS_H */
