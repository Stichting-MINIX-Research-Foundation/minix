/*	tty.h - Terminals	*/

#include <timers.h>

/* First minor numbers for the various classes of TTY devices. */
#define CONS_MINOR	   0
#define LOG_MINOR	  15
#define RS232_MINOR	  16
#define TTYPX_MINOR	 128
#define PTYPX_MINOR	 192

#define LINEWRAP	   1	/* console.c - wrap lines at column 80 */

#define TTY_IN_BYTES     256	/* tty input queue size */
#define TAB_SIZE           8	/* distance between tab stops */
#define TAB_MASK           7	/* mask to compute a tab stop position */

#define ESC             '\33'	/* escape */

#define O_NOCTTY       00400	/* from <fcntl.h>, or cc will choke */
#define O_NONBLOCK     04000

struct tty;
typedef _PROTOTYPE( int (*devfun_t), (struct tty *tp, int try_only) );
typedef _PROTOTYPE( void (*devfunarg_t), (struct tty *tp, int c) );

typedef struct tty {
  int tty_events;		/* set when TTY should inspect this line */
  int tty_index;		/* index into TTY table */
  int tty_minor;		/* device minor number */

  /* Input queue.  Typed characters are stored here until read by a program. */
  u16_t *tty_inhead;		/* pointer to place where next char goes */
  u16_t *tty_intail;		/* pointer to next char to be given to prog */
  int tty_incount;		/* # chars in the input queue */
  int tty_eotct;		/* number of "line breaks" in input queue */
  devfun_t tty_devread;		/* routine to read from low level buffers */
  devfun_t tty_icancel;		/* cancel any device input */
  int tty_min;			/* minimum requested #chars in input queue */
  timer_t tty_tmr;		/* the timer for this tty */

  /* Output section. */
  devfun_t tty_devwrite;	/* routine to start actual device output */
  devfunarg_t tty_echo;		/* routine to echo characters input */
  devfun_t tty_ocancel;		/* cancel any ongoing device output */
  devfun_t tty_break;		/* let the device send a break */

  /* Terminal parameters and status. */
  int tty_position;		/* current position on the screen for echoing */
  char tty_reprint;		/* 1 when echoed input messed up, else 0 */
  char tty_escaped;		/* 1 when LNEXT (^V) just seen, else 0 */
  char tty_inhibited;		/* 1 when STOP (^S) just seen (stops output) */
  char tty_pgrp;		/* slot number of controlling process */
  char tty_openct;		/* count of number of opens of this tty */

  /* Information about incomplete I/O requests is stored here. */
  char tty_inrepcode;		/* reply code, TASK_REPLY or REVIVE */
  char tty_inrevived;		/* set to 1 if revive callback is pending */
  char tty_incaller;		/* process that made the call (usually FS) */
  char tty_inproc;		/* process that wants to read from tty */
  vir_bytes tty_in_vir;		/* virtual address where data is to go */
  int tty_inleft;		/* how many chars are still needed */
  int tty_incum;		/* # chars input so far */
  char tty_outrepcode;		/* reply code, TASK_REPLY or REVIVE */
  char tty_outrevived;		/* set to 1 if revive callback is pending */
  char tty_outcaller;		/* process that made the call (usually FS) */
  char tty_outproc;		/* process that wants to write to tty */
  vir_bytes tty_out_vir;	/* virtual address where data comes from */
  int tty_outleft;		/* # chars yet to be output */
  int tty_outcum;		/* # chars output so far */
  char tty_iocaller;		/* process that made the call (usually FS) */
  char tty_ioproc;		/* process that wants to do an ioctl */
  int tty_ioreq;		/* ioctl request code */
  vir_bytes tty_iovir;		/* virtual address of ioctl buffer */

  /* select() data */
  int tty_select_ops;		/* which operations are interesting */
  int tty_select_proc;		/* which process wants notification */

  /* Miscellaneous. */
  devfun_t tty_ioctl;		/* set line speed, etc. at the device level */
  devfun_t tty_close;		/* tell the device that the tty is closed */
  void *tty_priv;		/* pointer to per device private data */
  struct termios tty_termios;	/* terminal attributes */
  struct winsize tty_winsize;	/* window size (#lines and #columns) */

  u16_t tty_inbuf[TTY_IN_BYTES];/* tty input buffer */

} tty_t;

/* Memory allocated in tty.c, so extern here. */
extern tty_t tty_table[NR_CONS+NR_RS_LINES+NR_PTYS];
extern int ccurrent;		/* currently visible console */
extern int irq_hook_id;		/* hook id for keyboard irq */

extern unsigned long kbd_irq_set;
extern unsigned long rs_irq_set;

/* Values for the fields. */
#define NOT_ESCAPED        0	/* previous character is not LNEXT (^V) */
#define ESCAPED            1	/* previous character was LNEXT (^V) */
#define RUNNING            0	/* no STOP (^S) has been typed to stop output */
#define STOPPED            1	/* STOP (^S) has been typed to stop output */

/* Fields and flags on characters in the input queue. */
#define IN_CHAR       0x00FF	/* low 8 bits are the character itself */
#define IN_LEN        0x0F00	/* length of char if it has been echoed */
#define IN_LSHIFT          8	/* length = (c & IN_LEN) >> IN_LSHIFT */
#define IN_EOT        0x1000	/* char is a line break (^D, LF) */
#define IN_EOF        0x2000	/* char is EOF (^D), do not return to user */
#define IN_ESC        0x4000	/* escaped by LNEXT (^V), no interpretation */

/* Times and timeouts. */
#define force_timeout()	((void) (0))

/* Memory allocated in tty.c, so extern here. */
extern timer_t *tty_timers;		/* queue of TTY timers */
extern clock_t tty_next_timeout;	/* next TTY timeout */

/* Number of elements and limit of a buffer. */
#define buflen(buf)	(sizeof(buf) / sizeof((buf)[0]))
#define bufend(buf)	((buf) + buflen(buf))

/* Memory allocated in tty.c, so extern here. */
extern struct machine machine;	/* machine information (a.o.: pc_at, ega) */

/* Function prototypes for TTY driver. */
/* tty.c */
_PROTOTYPE( void handle_events, (struct tty *tp)			);
_PROTOTYPE( void sigchar, (struct tty *tp, int sig)			);
_PROTOTYPE( void tty_task, (void)					);
_PROTOTYPE( int in_process, (struct tty *tp, char *buf, int count)	);
_PROTOTYPE( void out_process, (struct tty *tp, char *bstart, char *bpos,
				char *bend, int *icount, int *ocount)	);
_PROTOTYPE( void tty_wakeup, (clock_t now)				);
_PROTOTYPE( void tty_reply, (int code, int replyee, int proc_nr,
							int status)	);
_PROTOTYPE( int tty_devnop, (struct tty *tp, int try)			);
_PROTOTYPE( int select_try, (struct tty *tp, int ops)			);
_PROTOTYPE( int select_retry, (struct tty *tp)				);

/* rs232.c */
_PROTOTYPE( void rs_init, (struct tty *tp)				);
_PROTOTYPE( void rs_interrupt, (message *m)				);

#if (CHIP == INTEL)
/* console.c */
_PROTOTYPE( void kputc, (int c)						);
_PROTOTYPE( void cons_stop, (void)					);
_PROTOTYPE( void do_new_kmess, (message *m)				);
_PROTOTYPE( void do_diagnostics, (message *m)				);
_PROTOTYPE( void scr_init, (struct tty *tp)				);
_PROTOTYPE( void toggle_scroll, (void)					);
_PROTOTYPE( int con_loadfont, (message *m)				);
_PROTOTYPE( void select_console, (int cons_line)			);

/* keyboard.c */
_PROTOTYPE( void kb_init, (struct tty *tp)				);
_PROTOTYPE( void kb_init_once, (void)					);
_PROTOTYPE( int kbd_loadmap, (message *m)				);
_PROTOTYPE( void do_panic_dumps, (message *m)				);
_PROTOTYPE( void do_fkey_ctl, (message *m)				);
_PROTOTYPE( void kbd_interrupt, (message *m)				);

/* pty.c */
_PROTOTYPE( void do_pty, (struct tty *tp, message *m_ptr)		);
_PROTOTYPE( void pty_init, (struct tty *tp)				);
_PROTOTYPE( void select_retry_pty, (struct tty *tp)			);
_PROTOTYPE( int pty_status, (message *m_ptr)				);

/* vidcopy.s */
_PROTOTYPE( void vid_vid_copy, (unsigned src, unsigned dst, unsigned count));
_PROTOTYPE( void mem_vid_copy, (u16_t *src, unsigned dst, unsigned count));

#endif /* (CHIP == INTEL) */

