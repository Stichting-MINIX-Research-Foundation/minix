/* Constants */
#define	_SUBWIN		1		/* window is a subwindow */
#define	_ENDLINE	2		/* last winline is last screen line */
#define	_FULLWIN	4		/* window fills screen */
#define	_SCROLLWIN	8		/* window lwr rgt is screen lwr rgt */

#define	_NO_CHANGE	-1		/* flags line edge unchanged */
#define	_BREAKCHAR	0x03		/* ^C character */
#define _DCCHAR		0x08		/* Delete Char char (BS) */
#define _DLCHAR		0x1b		/* Delete Line char (ESC) */
#define	_GOCHAR		0x11		/* ^Q character */
#define	_PRINTCHAR	0x10		/* ^P character */
#define	_STOPCHAR	0x13		/* ^S character */
#define	 NUNGETCH	10		/* max # chars to ungetch() */

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

/* Character mask definitions. */
#define CHR_MSK	((int) 0x00ff)		/* ASCIIZ character mask */
#define	ATR_MSK	((int) 0xff00)		/* attribute mask */
#define ATR_NRM	((int) 0x0000)		/* no special attributes */

/* Type declarations. */

typedef	struct {
  WINDOW  *tmpwin;			/* window used for updates */
  int	   cursrow;			/* position of physical cursor */
  int	   curscol;
  bool     rawmode;
  bool     cbrkmode;
  bool     echoit;
} cursv;

/* External variables */
extern	cursv   _cursvar;		/* curses variables */
