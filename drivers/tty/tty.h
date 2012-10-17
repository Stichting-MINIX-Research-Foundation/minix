/*	tty.h - Terminals	*/

#include <timers.h>

#undef lock
#undef unlock

#define TTY_REVIVE	6767

/* First minor numbers for the various classes of TTY devices. */
#define CONS_MINOR	   0
#define LOG_MINOR	  15
#define RS232_MINOR	  16
#define KBD_MINOR	 127
#define KBDAUX_MINOR	 126
#define VIDEO_MINOR	 125
#define TTYPX_MINOR	 128
#define PTYPX_MINOR	 192

#define CONS_ARG	  30	/* console= boot param length (incl. nul) */
#define LINEWRAP	   1	/* console.c - wrap lines at column 80 */

#define TTY_IN_BYTES     256	/* tty input queue size */
#define TAB_SIZE           8	/* distance between tab stops */
#define TAB_MASK           7	/* mask to compute a tab stop position */

#define ESC             '\33'	/* escape */

#define O_NOCTTY       00400	/* from <fcntl.h>, or cc will choke */
#define O_NONBLOCK     04000

struct tty;
typedef int(*devfun_t) (struct tty *tp, int try_only);
typedef void(*devfunarg_t) (struct tty *tp, int c);

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
  endpoint_t tty_pgrp;		/* endpoint of controlling process */
  char tty_openct;		/* count of number of opens of this tty */

  /* Information about incomplete I/O requests is stored here. */
  int tty_inrepcode;		/* reply code, TASK_REPLY or REVIVE */
  char tty_inrevived;		/* set to 1 if revive callback is pending */
  endpoint_t tty_incaller;	/* process that made the call (usually VFS) */
  endpoint_t tty_inproc;	/* process that wants to read from tty */
  cp_grant_id_t tty_ingrant;	/* grant where data is to go */
  vir_bytes tty_inoffset;	/* offset into grant */
  int tty_inleft;		/* how many chars are still needed */
  int tty_incum;		/* # chars input so far */
  int tty_outrepcode;		/* reply code, TASK_REPLY or REVIVE */
  int tty_outrevived;		/* set to 1 if revive callback is pending */
  endpoint_t tty_outcaller;	/* process that made the call (usually VFS) */
  endpoint_t tty_outproc;	/* process that wants to write to tty */
  cp_grant_id_t tty_outgrant;	/* grant where data comes from */
  vir_bytes tty_outoffset;	/* offset into grant */
  int tty_outleft;		/* # chars yet to be output */
  int tty_outcum;		/* # chars output so far */
  endpoint_t tty_iocaller;	/* process that made the call (usually VFS) */
  int tty_iorevived;		/* set to 1 if revive callback is pending */
  endpoint_t tty_ioproc;	/* process that wants to do an ioctl */
  int tty_iostatus;		/* result */
  int tty_ioreq;		/* ioctl request code */
  cp_grant_id_t tty_iogrant;	/* virtual address of ioctl buffer or grant */

  /* select() data */
  int tty_select_ops;		/* which operations are interesting */
  endpoint_t tty_select_proc;	/* which process wants notification */

  /* Miscellaneous. */
  devfun_t tty_ioctl;		/* set line speed, etc. at the device level */
  devfun_t tty_open;		/* tell the device that the tty is opened */ 
  devfun_t tty_close;		/* tell the device that the tty is closed */
  void *tty_priv;		/* pointer to per device private data */
  struct termios tty_termios;	/* terminal attributes */
  struct winsize tty_winsize;	/* window size (#lines and #columns) */

  u16_t tty_inbuf[TTY_IN_BYTES];/* tty input buffer */

} tty_t;

/* Memory allocated in tty.c, so extern here. */
extern tty_t tty_table[NR_CONS+NR_RS_LINES+NR_PTYS];
extern int ccurrent;		/* currently visible console */
extern u32_t system_hz;		/* system clock frequency */

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

/* Number of elements and limit of a buffer. */
#define buflen(buf)	(sizeof(buf) / sizeof((buf)[0]))
#define bufend(buf)	((buf) + buflen(buf))

/* Memory allocated in tty.c, so extern here. */
extern struct machine machine;	/* machine information (a.o.: pc_at, ega) */

/* The tty outputs diagnostic messages in a circular buffer. */
extern struct kmessages kmess;

/* Function prototypes for TTY driver. */
/* tty.c */
void handle_events(struct tty *tp);
void sigchar(struct tty *tp, int sig, int mayflush);
void tty_task(void);
int in_process(struct tty *tp, char *buf, int count, int scode);
void out_process(struct tty *tp, char *bstart, char *bpos, char *bend,
	int *icount, int *ocount);
void tty_wakeup(clock_t now);
#define tty_reply(c, r, p, s) tty_reply_f(__FILE__, __LINE__, (c), (r), (p), (s))
void tty_reply_f(char *f, int l, int code, int replyee, int proc_nr, int
	status);
int select_try(struct tty *tp, int ops);
int select_retry(struct tty *tp);

/* rs232.c */
void rs_init(struct tty *tp);
void rs_interrupt(message *m);

/* console.c */
void kputc(int c);
void cons_stop(void);
void scr_init(struct tty *tp);
void toggle_scroll(void);
int con_loadfont(message *m);
void select_console(int cons_line);
void beep_x( unsigned freq, clock_t dur);
void do_video(message *m);

/* keyboard.c */
void kb_init(struct tty *tp);
void kb_init_once(void);
int kbd_loadmap(message *m);
void do_fkey_ctl(message *m);
void kbd_interrupt(message *m);
void do_kbd(message *m);
void do_kb_inject(message *m);
void do_kbdaux(message *m);
int kbd_status(message *m_ptr);

/* pty.c */
void do_pty(struct tty *tp, message *m_ptr);
void pty_init(struct tty *tp);
void select_retry_pty(struct tty *tp);
int pty_status(message *m_ptr);

