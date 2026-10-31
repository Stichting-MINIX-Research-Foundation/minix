/* 
 * dummy serial console output
 */

#include <minix/drivers.h>
#include <termios.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/video.h>
#include <sys/mman.h>
#include <sys/termios.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/sys_config.h>
#include <minix/vm.h>
#include "tty.h"
#include "console.h"

/* Private variables used by the console driver. */

static int disabled_vc = -1;	/* Virtual console that was active when 
				 * disable_console was called.
				 */

/* Per console data. */
typedef struct console {
  tty_t *c_tty;			/* associated TTY struct */
  int c_column;			/* current column number (0-origin) */
  int c_row;			/* current row (0 at top of screen) */
  int c_rwords;			/* number of WORDS (not bytes) in outqueue */
  int c_line;			/* line no */
} console_t;


static int nr_cons= 1;		/* actual number of consoles */
static console_t cons_table[NR_CONS];
static console_t *curcons = NULL;	/* currently visible */

static int shutting_down = FALSE;	/* don't allow console switches */

static int cons_write(struct tty *tp, int try);
static void cons_echo(tty_t *tp, int c);
static void flush(console_t *cons);
static void disable_console(void);
static void reenable_console(void);
static void stop_beep(int arg);
static int cons_ioctl(tty_t *tp, int);

static void ser_cons_stop(void);
static void ser_scr_init(struct tty *tp);
static int ser_con_loadfont(endpoint_t endpt, cp_grant_id_t grant);
static void ser_select_console(int cons_line);

cons_sw_t ser_cons_sw = {
	ser_scr_init,
	ser_con_loadfont,
	ser_cons_stop,
	ser_select_console,
	disable_console,
	reenable_console
};

/*===========================================================================*
 *				cons_write				     *
 *				tp tells which terminal is to be used *
 *===========================================================================*/
static int cons_write(register struct tty *tp, int try)
{
/* Copy as much data as possible to the output queue, then start I/O.  On
 * memory-mapped terminals, such as the IBM console, the I/O will also be
 * finished, and the counts updated.  Keep repeating until all I/O done.
 */

  int count;
  int result = OK;
  register char *tbuf;
  char buf[64];
  console_t *cons = tp->tty_priv;

  if (try) return 1;	/* we can always write to console */

  /* Check quickly for nothing to do, so this can be called often without
   * unmodular tests elsewhere.
   */
  if ((count = tp->tty_outleft) == 0 || tp->tty_inhibited) return 0;

  /* Copy the user bytes to buf[] for decent addressing. Loop over the
   * copies, since the user buffer may be much larger than buf[].
   */
  do {
	if (count > sizeof(buf)) count = sizeof(buf);
	if (tp->tty_outcaller == KERNEL) {
		/* We're trying to print on kernel's behalf */
		memcpy(buf, (char *) tp->tty_outgrant + tp->tty_outcum, count);
	} else {
		if ((result = sys_safecopyfrom(tp->tty_outcaller,
				tp->tty_outgrant, tp->tty_outcum,
				(vir_bytes) buf, count)) != OK) {
			break;
		}
	}
	tbuf = buf;

	/* Update terminal data structure. */
	tp->tty_outcum += count;
	tp->tty_outleft -= count;

	/* Output each byte of the copy to the screen.  Avoid calling
	 * out_char() for the "easy" characters, put them into the buffer
	 * directly.
	 */
	do {
		ser_putc(*tbuf++);
	} while (--count != 0);
  } while ((count = tp->tty_outleft) != 0 && !tp->tty_inhibited);

  flush(cons);			/* transfer anything buffered to the screen */

  /* Reply to the writer if all output is finished or if an error occurred. */
  if (tp->tty_outleft == 0 || result != OK) {
	if (tp->tty_outcaller != KERNEL)
		chardriver_reply_task(tp->tty_outcaller, tp->tty_outid,
			result != OK ? result : tp->tty_outcum);
	tp->tty_outcum = tp->tty_outleft = 0;
	tp->tty_outcaller = NONE;
  }

  return 0;
}

/*===========================================================================*
 *				cons_echo				     *
 *				tp pointer to tty struct
 *				c character to be echoed
 *===========================================================================*/
static void cons_echo(register tty_t *tp, int c)
{
/* Echo keyboard input (print & flush). */
  console_t *cons = tp->tty_priv;

  ser_putc(c);
  flush(cons);
}

/*===========================================================================*
 *				flush					     *
 *				cons - pointer to console struct
 *===========================================================================*/
static void flush(register console_t *cons)
{
  tty_t *tp = cons->c_tty;
}

/*===========================================================================*
 *				scr_init				     *
 *===========================================================================*/
static void ser_scr_init(tty_t *tp)
{
/* Initialize the screen driver. */
  console_t *cons;
  int line;

  /* Associate console and TTY. */
  line = tp - &tty_table[0];
  if (line >= nr_cons) return;
  cons = &cons_table[line];
  cons->c_tty = tp;
  cons->c_line = line;
  tp->tty_priv = cons;

  /* Fill in TTY function hooks. */
  tp->tty_devwrite = cons_write;
  tp->tty_echo = cons_echo;
  tp->tty_ioctl = cons_ioctl;

  select_console(0);
  cons_ioctl(tp, 0);
}

/*===========================================================================*
 *				cons_stop				     *
 *===========================================================================*/
static void ser_cons_stop(void)
{
/* Prepare for halt or reboot. */
  select_console(0);
  shutting_down = TRUE;
}

/*===========================================================================*
 *				disable_console				     *
 *===========================================================================*/
static void disable_console(void)
{
	if (disabled_vc != -1)
		return;
	
	disabled_vc = ccurrent;

	select_console(0);

	/* Should also disable further output to virtual consoles */
}

/*===========================================================================*
 *				reenable_console			     *
 *===========================================================================*/
static void reenable_console(void)
{
	if (disabled_vc == -1)
		return;

	select_console(disabled_vc);
	disabled_vc = -1;
}

/*===========================================================================*
 *				select_console				     *
 *===========================================================================*/
static void ser_select_console(int cons_line)
{
/* Set the current console to console number 'cons_line'. */

  if (shutting_down) return;

  if (cons_line < 0 || cons_line >= nr_cons) return;

  ccurrent = cons_line;
  curcons = &cons_table[cons_line];

}

/*===========================================================================*
 *				con_loadfont				     *
 *===========================================================================*/
static int ser_con_loadfont(endpoint_t endpt, cp_grant_id_t grant)
{
  return OK;
}

/*===========================================================================*
 *				cons_ioctl				     *
 *===========================================================================*/
static int cons_ioctl(tty_t *tp, int UNUSED(try))
{
/* Set the screen dimensions. */
  return 0;
}

