/*
 * System wide defaults for terminal state.
 */
#ifndef _SYS_TTYDEFAULTS_H_
#define	_SYS_TTYDEFAULTS_H_

/* NetBSD-like definition of values aready set up in termios.h */

/*
 * Defaults on "first" open.
 */
#define	TTYDEF_IFLAG	(BRKINT | ICRNL | IXON | IXANY)
#define TTYDEF_OFLAG	(OPOST | ONLCR )
#define TTYDEF_LFLAG	(ECHO | ICANON | ISIG | IEXTEN | ECHOE)
#define TTYDEF_CFLAG	(CREAD | CS8 | HUPCL)
#define TTYDEF_SPEED	(B9600)

/*
 * Control Character Defaults
 */
#define CTRL(x)	(x&037)
#define	CEOF		CTRL('d')
#define	CEOL		_POSIX_VDISABLE
#define	CERASE		CTRL('h')
#define	CINTR		CTRL('c')
#define	CSTATUS		CTRL('t')
#define	CKILL		CTRL('u')
#define	CMIN		1
#define	CQUIT		034		/* FS, ^\ */
#define	CSUSP		CTRL('z')
#define	CTIME		0
#define	CDSUSP		CTRL('y')
#define	CSTART		CTRL('q')
#define	CSTOP		CTRL('s')
#define	CLNEXT		CTRL('v')
#define	CDISCARD 	CTRL('o')
#define	CWERASE 	CTRL('w')
#define	CREPRINT 	CTRL('r')
#define	CEOT		CEOF
/* compat */
#define	CBRK		CEOL
#define CRPRNT		CREPRINT
#define	CFLUSH		CDISCARD

#endif /* _SYS_TTYDEFAULTS_H_ */
