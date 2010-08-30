/* Keyboard driver for PC's and AT's.
 *
 * Changes: 
 *   Jul 13, 2004   processes can observe function keys  (Jorrit N. Herder)
 *   Jun 15, 2004   removed wreboot(), except panic dumps (Jorrit N. Herder)
 *   Feb 04, 1994   loadable keymaps  (Marcus Hampel)
 */

#include <minix/drivers.h>
#include <sys/ioctl.h>
#include <sys/kbdio.h>
#include <sys/time.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <machine/archtypes.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include "tty.h"
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"
#include "kernel/proc.h"

PRIVATE u16_t keymap[NR_SCAN_CODES * MAP_COLS] = {
#include "keymaps/us-std.src"
};

PRIVATE u16_t keymap_escaped[NR_SCAN_CODES * MAP_COLS] = {
#include "keymaps/us-std-esc.src"
};

PRIVATE int irq_hook_id = -1;
PRIVATE int aux_irq_hook_id = -1;

/* Standard and AT keyboard.  (PS/2 MCA implies AT throughout.) */
#define KEYBD		0x60	/* I/O port for keyboard data */

/* AT keyboard. */
#define KB_COMMAND	0x64	/* I/O port for commands on AT */
#define KB_STATUS	0x64	/* I/O port for status on AT */
#define KB_ACK		0xFA	/* keyboard ack response */
#define KB_AUX_BYTE	0x20	/* Auxiliary Device Output Buffer Full */
#define KB_OUT_FULL	0x01	/* status bit set when keypress char pending */
#define KB_IN_FULL	0x02	/* status bit set when not ready to receive */
#define KBC_RD_RAM_CCB	0x20	/* Read controller command byte */
#define KBC_WR_RAM_CCB	0x60	/* Write controller command byte */
#define KBC_DI_AUX	0xA7	/* Disable Auxiliary Device */
#define KBC_EN_AUX	0xA8	/* Enable Auxiliary Device */
#define KBC_DI_KBD	0xAD	/* Disable Keybard Interface */
#define KBC_EN_KBD	0xAE	/* Enable Keybard Interface */
#define KBC_WRITE_AUX	0xD4	/* Write to Auxiliary Device */
#define LED_CODE	0xED	/* command to keyboard to set LEDs */
#define MAX_KB_ACK_RETRIES 0x1000	/* max #times to wait for kb ack */
#define MAX_KB_BUSY_RETRIES 0x1000	/* max #times to loop while kb busy */
#define KBIT		0x80	/* bit used to ack characters to keyboard */

#define KBC_IN_DELAY	7	/* wait 7 microseconds when polling */

/* Miscellaneous. */
#define ESC_SCAN	0x01	/* reboot key when panicking */
#define SLASH_SCAN	0x35	/* to recognize numeric slash */
#define RSHIFT_SCAN	0x36	/* to distinguish left and right shift */
#define HOME_SCAN	0x47	/* first key on the numeric keypad */
#define INS_SCAN	0x52	/* INS for use in CTRL-ALT-INS reboot */
#define DEL_SCAN	0x53	/* DEL for use in CTRL-ALT-DEL reboot */

#define KBD_BUFSZ	1024	/* Buffer size for raw scan codes */
#define KBD_OUT_BUFSZ	16	/* Output buffer to sending data to the
				 * keyboard.
				 */

#define CONSOLE		   0	/* line number for console */
#define KB_IN_BYTES	  32	/* size of keyboard input buffer */
PRIVATE char ibuf[KB_IN_BYTES];	/* input buffer */
PRIVATE char *ihead = ibuf;	/* next free spot in input buffer */
PRIVATE char *itail = ibuf;	/* scan code to return to TTY */
PRIVATE int icount;		/* # codes in buffer */

PRIVATE int esc;		/* escape scan code detected? */
PRIVATE int alt_l;		/* left alt key state */
PRIVATE int alt_r;		/* right alt key state */
PRIVATE int alt;		/* either alt key */
PRIVATE int ctrl_l;		/* left control key state */
PRIVATE int ctrl_r;		/* right control key state */
PRIVATE int ctrl;		/* either control key */
PRIVATE int shift_l;		/* left shift key state */
PRIVATE int shift_r;		/* right shift key state */
PRIVATE int shift;		/* either shift key */
PRIVATE int num_down;		/* num lock key depressed */
PRIVATE int caps_down;		/* caps lock key depressed */
PRIVATE int scroll_down;	/* scroll lock key depressed */
PRIVATE int alt_down;	        /* alt key depressed */
PRIVATE int locks[NR_CONS];	/* per console lock keys state */

/* Lock key active bits.  Chosen to be equal to the keyboard LED bits. */
#define SCROLL_LOCK	0x01
#define NUM_LOCK	0x02
#define CAPS_LOCK	0x04
#define ALT_LOCK	0x08

PRIVATE char numpad_map[] =
		{'H', 'Y', 'A', 'B', 'D', 'C', 'V', 'U', 'G', 'S', 'T', '@'};

PRIVATE char *fkey_map[] =
		{"11", "12", "13", "14", "15", "17",	/* F1-F6 */
		 "18", "19", "20", "21", "23", "24"};	/* F7-F12 */

/* Variables and definition for observed function keys. */
typedef struct observer { int proc_nr; int events; } obs_t;
PRIVATE obs_t  fkey_obs[12];	/* observers for F1-F12 */
PRIVATE obs_t sfkey_obs[12];	/* observers for SHIFT F1-F12 */

PRIVATE struct kbd
{
	int minor;
	int nr_open;
	char buf[KBD_BUFSZ];
	int offset;
	int avail;
	int req_size;
	int req_proc;
	int req_safe;		/* nonzero: safe (req_addr_g is grant) */
	vir_bytes req_addr_g;	/* Virtual address or grant */
	vir_bytes req_addr_offset;
	int incaller;
	int select_ops;
	int select_proc;
} kbd, kbdaux;

/* Data that is to be sent to the keyboard. Each byte is ACKed by the
 * keyboard.
 */
PRIVATE struct kbd_outack
{
	unsigned char buf[KBD_OUT_BUFSZ];
	int offset;
	int avail;
	int expect_ack;
} kbdout;

PRIVATE int kbd_watchdog_set= 0;
PRIVATE int kbd_alive= 1;
PRIVATE long sticky_alt_mode = 0;
PRIVATE long debug_fkeys = 1;
PRIVATE timer_t tmr_kbd_wd;

FORWARD _PROTOTYPE( void handle_req, (struct kbd *kbdp, message *m)	);
FORWARD _PROTOTYPE( int handle_status, (struct kbd *kbdp, message *m)	);
FORWARD _PROTOTYPE( void kbc_cmd0, (int cmd)				);
FORWARD _PROTOTYPE( void kbc_cmd1, (int cmd, int data)			);
FORWARD _PROTOTYPE( int kbc_read, (void)				);
FORWARD _PROTOTYPE( void kbd_send, (void)				);
FORWARD _PROTOTYPE( int kb_ack, (void) 					);
FORWARD _PROTOTYPE( int kb_wait, (void)				 	);
FORWARD _PROTOTYPE( int func_key, (int scode) 				);
FORWARD _PROTOTYPE( int scan_keyboard, (unsigned char *bp, int *isauxp)	);
FORWARD _PROTOTYPE( unsigned make_break, (int scode) 			);
FORWARD _PROTOTYPE( void set_leds, (void) 				);
FORWARD _PROTOTYPE( void show_key_mappings, (void) 			);
FORWARD _PROTOTYPE( int kb_read, (struct tty *tp, int try) 		);
FORWARD _PROTOTYPE( unsigned map_key, (int scode) 			);
FORWARD _PROTOTYPE( void kbd_watchdog, (timer_t *tmrp)			);

int micro_delay(u32_t usecs)
{
	/* TTY can't use the library micro_delay() as that calls PM. */
	tickdelay(micros_to_ticks(usecs));
	return OK;
}

/*===========================================================================*
 *				do_kbd					     *
 *===========================================================================*/
PUBLIC void do_kbd(message *m)
{
	handle_req(&kbd, m);
}


/*===========================================================================*
 *				kbd_status				     *
 *===========================================================================*/
PUBLIC int kbd_status(message *m)
{
	int r;

	r= handle_status(&kbd, m);
	if (r)
		return r;
	return handle_status(&kbdaux, m);
}


/*===========================================================================*
 *				do_kbdaux				     *
 *===========================================================================*/
PUBLIC void do_kbdaux(message *m)
{
	handle_req(&kbdaux, m);
}


/*===========================================================================*
 *				handle_req				     *
 *===========================================================================*/
PRIVATE void handle_req(kbdp, m)
struct kbd *kbdp;
message *m;
{
	int i, n, r, ops, watch, safecopy = 0;
	unsigned char c;

	/* Execute the requested device driver function. */
	r= EINVAL;	/* just in case */
	switch (m->m_type) {
	    case DEV_OPEN:
		kbdp->nr_open++;
		r= OK;
		break;
	    case DEV_CLOSE:
		kbdp->nr_open--;
		if (kbdp->nr_open < 0)
		{
			printf("TTY(kbd): open count is negative\n");
			kbdp->nr_open= 0;
		}
		if (kbdp->nr_open == 0)
			kbdp->avail= 0;
		r= OK;
		break;
	    case DEV_READ_S:
	        safecopy = 1;
		if (kbdp->req_size)
		{
			/* We handle only request at a time */
			r= EIO;
			break;
		}
		if (kbdp->avail == 0)
		{
			/* Should record proc */
			kbdp->req_size= m->COUNT;
			kbdp->req_proc= m->IO_ENDPT;
			kbdp->req_addr_g= (vir_bytes)m->ADDRESS;
			kbdp->req_addr_offset= 0;
			kbdp->req_safe= safecopy;
			kbdp->incaller= m->m_source;
			r= SUSPEND;
			break;
		}

		/* Handle read request */
		n= kbdp->avail;
		if (n > m->COUNT)
			n= m->COUNT;
		if (kbdp->offset + n > KBD_BUFSZ)
			n= KBD_BUFSZ-kbdp->offset;
		if (n <= 0)
			panic("do_kbd(READ): bad n: %d", n);
		if(safecopy) {
		  r= sys_safecopyto(m->IO_ENDPT, (vir_bytes) m->ADDRESS, 0, 
			(vir_bytes) &kbdp->buf[kbdp->offset], n, D);
		} else {
		  r= sys_vircopy(SELF, D, (vir_bytes)&kbdp->buf[kbdp->offset], 
			m->IO_ENDPT, D, (vir_bytes) m->ADDRESS, n);
		}
		if (r == OK)
		{
			kbdp->offset= (kbdp->offset+n) % KBD_BUFSZ;
			kbdp->avail -= n;
			r= n;
		} else {
			printf("copy in read kbd failed: %d\n", r);
		}

		break;

	    case DEV_WRITE_S:
	        safecopy = 1;
		if (kbdp != &kbdaux)
		{
			printf("write to keyboard not implemented\n");
			r= EINVAL;
			break;
		}

		/* Assume that output to AUX only happens during
		 * initialization and we can afford to lose input. This should
		 * be fixed at a later time.
		 */
		for (i= 0; i<m->COUNT; i++)
		{
			if(safecopy) {
		  	  r= sys_safecopyfrom(m->IO_ENDPT, (vir_bytes)
				m->ADDRESS, i, (vir_bytes)&c, 1, D);
			} else {
			  r= sys_vircopy(m->IO_ENDPT, D,
				(vir_bytes) m->ADDRESS+i,
				SELF, D, (vir_bytes)&c, 1);
			}
			if (r != OK)
				break;
			kbc_cmd1(KBC_WRITE_AUX, c);
		}
		r= i;
		break;

	    case CANCEL:
		kbdp->req_size= 0;
		r= OK;
		break;
	    case DEV_SELECT:
		ops = m->IO_ENDPT & (SEL_RD|SEL_WR|SEL_ERR);
		watch = (m->IO_ENDPT & SEL_NOTIFY) ? 1 : 0;
		
		r= 0;
		if (kbdp->avail && (ops & SEL_RD))
		{
			r |= SEL_RD;
			break;
		}

		if (ops && watch)
		{
			kbdp->select_ops |= ops;
			kbdp->select_proc= m->m_source;
		}
		break;
	    case DEV_IOCTL_S:
		 safecopy=1;
		if (kbdp == &kbd && m->TTY_REQUEST == KIOCSLEDS)
		{
			kio_leds_t leds;
			unsigned char b;

			
			if(safecopy) {
		  	  r= sys_safecopyfrom(m->IO_ENDPT, (vir_bytes)
				m->ADDRESS, 0, (vir_bytes)&leds,
				sizeof(leds), D);
			} else {
			 r= sys_vircopy(m->IO_ENDPT, D, (vir_bytes) m->ADDRESS,
				SELF, D, (vir_bytes)&leds, sizeof(leds));
			}
			if (r != OK)
				break;
			b= 0;
			if (leds.kl_bits & KBD_LEDS_NUM) b |= NUM_LOCK;
			if (leds.kl_bits & KBD_LEDS_CAPS) b |= CAPS_LOCK;
			if (leds.kl_bits & KBD_LEDS_SCROLL) b |= SCROLL_LOCK;
			if (kbdout.avail == 0)
				kbdout.offset= 0;
			if (kbdout.offset + kbdout.avail + 2 > KBD_OUT_BUFSZ)
			{
				/* Output buffer is full. Ignore this command.
				 * Reset ACK flag.
				 */
				kbdout.expect_ack= 0;
			}
			else
			{
				kbdout.buf[kbdout.offset+kbdout.avail]=
					LED_CODE;
				kbdout.buf[kbdout.offset+kbdout.avail+1]= b;
				kbdout.avail += 2;
			 }
			 if (!kbdout.expect_ack)
				kbd_send();
			 r= OK;
			 break;
		}
		if (kbdp == &kbd && m->TTY_REQUEST == KIOCBELL)
		{
			kio_bell_t bell;
			clock_t ticks;

			if(safecopy) {
		  	  r= sys_safecopyfrom(m->IO_ENDPT, (vir_bytes)
				m->ADDRESS, 0, (vir_bytes)&bell,
				sizeof(bell), D);
			} else {
			  r= sys_vircopy(m->IO_ENDPT, D, (vir_bytes) m->ADDRESS,
				SELF, D, (vir_bytes)&bell, sizeof(bell));
			}
			if (r != OK)
				break;

			ticks= bell.kb_duration.tv_usec * system_hz / 1000000;
			ticks += bell.kb_duration.tv_sec * system_hz;
			if (!ticks)
				ticks++;
			beep_x(bell.kb_pitch, ticks);

			r= OK;
			break;
		}
		r= ENOTTY;
		break;

	    default:		
		printf("Warning, TTY(kbd) got unexpected request %d from %d\n",
			m->m_type, m->m_source);
		r= EINVAL;
	}
	tty_reply(TASK_REPLY, m->m_source, m->IO_ENDPT, r);
}


/*===========================================================================*
 *				handle_status				     *
 *===========================================================================*/
PRIVATE int handle_status(kbdp, m)
struct kbd *kbdp;
message *m;
{
	int n, r;

	if (kbdp->avail && kbdp->req_size && m->m_source == kbdp->incaller)
	{
		/* Handle read request */
		n= kbdp->avail;
		if (n > kbdp->req_size)
			n= kbdp->req_size;
		if (kbdp->offset + n > KBD_BUFSZ)
			n= KBD_BUFSZ-kbdp->offset;
		if (n <= 0)
			panic("kbd_status: bad n: %d", n);
		kbdp->req_size= 0;
		if(kbdp->req_safe) {
		  r= sys_safecopyto(kbdp->req_proc, kbdp->req_addr_g, 0,
			(vir_bytes)&kbdp->buf[kbdp->offset], n, D);
		} else {
		  r= sys_vircopy(SELF, D, (vir_bytes)&kbdp->buf[kbdp->offset], 
			kbdp->req_proc, D, kbdp->req_addr_g, n);
		}
		if (r == OK)
		{
			kbdp->offset= (kbdp->offset+n) % KBD_BUFSZ;
			kbdp->avail -= n;
			r= n;
		} else printf("copy in revive kbd failed: %d\n", r);

		m->m_type = DEV_REVIVE;
  		m->REP_ENDPT= kbdp->req_proc;
  		m->REP_IO_GRANT= kbdp->req_addr_g;
  		m->REP_STATUS= r;
		return 1;
	}
	if (kbdp->avail && (kbdp->select_ops & SEL_RD) &&
		m->m_source == kbdp->select_proc)
	{
		m->m_type = DEV_IO_READY;
		m->DEV_MINOR = kbdp->minor;
		m->DEV_SEL_OPS = SEL_RD;

		kbdp->select_ops &= ~SEL_RD;
		return 1;
	}

	return 0;
}


/*===========================================================================*
 *				map_key					     *
 *===========================================================================*/
PRIVATE unsigned map_key(scode)
int scode;
{
/* Map a scan code to an ASCII code. */

  int caps, column, lk;
  u16_t *keyrow;

  if(esc)
	  keyrow = &keymap_escaped[scode * MAP_COLS];
  else
	  keyrow = &keymap[scode * MAP_COLS];

  caps = shift;
  lk = locks[ccurrent];
  if ((lk & NUM_LOCK) && HOME_SCAN <= scode && scode <= DEL_SCAN) caps = !caps;
  if ((lk & CAPS_LOCK) && (keyrow[0] & HASCAPS)) caps = !caps;

  if (alt) {
	column = 2;
	if (ctrl || alt_r) column = 3;	/* Ctrl + Alt == AltGr */
	if (caps) column = 4;
  } else {
	if (sticky_alt_mode && (lk & ALT_LOCK)) {
		column = 2;
		if (caps) column = 4;
        } else {
		column = 0;
		if (caps) column = 1;
		if (ctrl) column = 5;
        }
  }
  return keyrow[column] & ~HASCAPS;
}

/*===========================================================================*
 *				kbd_interrupt				     *
 *===========================================================================*/
PUBLIC void kbd_interrupt(m_ptr)
message *m_ptr;
{
/* A keyboard interrupt has occurred.  Process it. */
  int o, isaux;
  unsigned char scode;
  struct kbd *kbdp;

  /* Fetch the character from the keyboard hardware and acknowledge it. */
  if (!scan_keyboard(&scode, &isaux))
	return;

  if (isaux)
	kbdp= &kbdaux;
  else if (kbd.nr_open)
	kbdp= &kbd;
  else
	kbdp= NULL;

  if (kbdp)
  {
	/* raw scan codes or aux data */
	if (kbdp->avail >= KBD_BUFSZ)
	{
#if 0
		printf("kbd_interrupt: %s buffer is full\n",
			isaux ? "kbdaux" : "keyboard");
#endif
		return;	/* Buffer is full */
	}
	 o= (kbdp->offset + kbdp->avail) % KBD_BUFSZ;
	 kbdp->buf[o]= scode;
	 kbdp->avail++;
	 if (kbdp->req_size) {
		notify(kbdp->incaller);
	 }
	 if (kbdp->select_ops & SEL_RD)
		notify(kbdp->select_proc);
	 return;
  }

  /* Store the scancode in memory so the task can get at it later. */
  if (icount < KB_IN_BYTES) {
	*ihead++ = scode;
	if (ihead == ibuf + KB_IN_BYTES) ihead = ibuf;
	icount++;
	tty_table[ccurrent].tty_events = 1;
	if (tty_table[ccurrent].tty_select_ops & SEL_RD) {
		select_retry(&tty_table[ccurrent]);
	}
  }
}

/*===========================================================================*
 *				kb_read					     *
 *===========================================================================*/
PRIVATE int kb_read(tp, try)
tty_t *tp;
int try;
{
/* Process characters from the circular keyboard buffer. */
  char buf[7], *p, suffix;
  int scode;
  unsigned ch;

  tp = &tty_table[ccurrent];		/* always use the current console */

  if (try) {
  	if (icount > 0) return 1;
  	return 0;
  }

  while (icount > 0) {
	scode = *itail++;			/* take one key scan code */
	if (itail == ibuf + KB_IN_BYTES) itail = ibuf;
	icount--;

	/* Function keys are being used for debug dumps (if enabled). */
	if (debug_fkeys && func_key(scode)) continue;

	/* Perform make/break processing. */
	ch = make_break(scode);

	if (ch <= 0xFF) {
		/* A normal character. */
		buf[0] = ch;
		(void) in_process(tp, buf, 1, scode);
	} else
	if (HOME <= ch && ch <= INSRT) {
		/* An ASCII escape sequence generated by the numeric pad. */
		buf[0] = ESC;
		buf[1] = '[';
		buf[2] = numpad_map[ch - HOME];
		(void) in_process(tp, buf, 3, scode);
	} else
	if ((F1 <= ch && ch <= F12) || (SF1 <= ch && ch <= SF12) ||
				(CF1 <= ch && ch <= CF12 && !debug_fkeys)) {
		/* An escape sequence generated by function keys. */
		if (F1 <= ch && ch <= F12) {
			ch -= F1;
			suffix = 0;
		} else
		if (SF1 <= ch && ch <= SF12) {
			ch -= SF1;
			suffix = '2';
		} else
		if (CF1 <= ch && ch <= CF12) {
			ch -= CF1;
			suffix = shift ? '6' : '5';
		}
		/* ^[[11~ for F1, ^[[24;5~ for CF12 etc */
		buf[0] = ESC;
		buf[1] = '[';
		buf[2] = fkey_map[ch][0];
		buf[3] = fkey_map[ch][1];
		p = &buf[4];
		if (suffix) {
			*p++ = ';';
			*p++ = suffix;
		}
		*p++ = '~';
		(void) in_process(tp, buf, p - buf, scode);
	} else
	if (ch == ALEFT) {
		/* Choose lower numbered console as current console. */
		select_console(ccurrent - 1);
		set_leds();
	} else
	if (ch == ARIGHT) {
		/* Choose higher numbered console as current console. */
		select_console(ccurrent + 1);
		set_leds();
	} else
	if (AF1 <= ch && ch <= AF12) {
		/* Alt-F1 is console, Alt-F2 is ttyc1, etc. */
		select_console(ch - AF1);
		set_leds();
	} else
	if (CF1 <= ch && ch <= CF12) {
	    switch(ch) {
  		case CF1: show_key_mappings(); break; 
  		case CF3: toggle_scroll(); break; /* hardware <-> software */	
  		case CF7: sigchar(&tty_table[CONSOLE], SIGQUIT, 1); break;
  		case CF8: sigchar(&tty_table[CONSOLE], SIGINT, 1); break;
  		case CF9: sigchar(&tty_table[CONSOLE], SIGKILL, 1); break;
  	    }
	} else {
		/* pass on scancode even though there is no character code */
		(void) in_process(tp, NULL, 0, scode);
	}
  }

  return 1;
}

/*===========================================================================*
 *				kbd_send				     *
 *===========================================================================*/
PRIVATE void kbd_send()
{
	unsigned long sb;
	int r;
	clock_t now;

	if (!kbdout.avail)
		return;
	if (kbdout.expect_ack)
		return;

	if((r=sys_inb(KB_STATUS, &sb)) != OK) {
		printf("kbd_send: 1 sys_inb() failed: %d\n", r);
	}
	if (sb & (KB_OUT_FULL|KB_IN_FULL))
	{
		printf("not sending 1: sb = 0x%lx\n", sb);
		return;
	}
	micro_delay(KBC_IN_DELAY);
	if((r=sys_inb(KB_STATUS, &sb)) != OK) {
		printf("kbd_send: 2 sys_inb() failed: %d\n", r);
	}
	if (sb & (KB_OUT_FULL|KB_IN_FULL))
	{
		printf("not sending 2: sb = 0x%lx\n", sb);
		return;
	}

	/* Okay, buffer is really empty */
#if 0
	printf("sending byte 0x%x to keyboard\n", kbdout.buf[kbdout.offset]);
#endif
	if((r=sys_outb(KEYBD, kbdout.buf[kbdout.offset])) != OK) {
		printf("kbd_send: 3 sys_inb() failed: %d\n", r);
	}
	kbdout.offset++;
	kbdout.avail--;
	kbdout.expect_ack= 1;

	kbd_alive= 1;
	if (kbd_watchdog_set)
	{
		/* Set a watchdog timer for one second. */
		set_timer(&tmr_kbd_wd, system_hz, kbd_watchdog, 0);

		kbd_watchdog_set= 1;
	 }
}

/*===========================================================================*
 *				make_break				     *
 *===========================================================================*/
PRIVATE unsigned make_break(scode)
int scode;			/* scan code of key just struck or released */
{
/* This routine can handle keyboards that interrupt only on key depression,
 * as well as keyboards that interrupt on key depression and key release.
 * For efficiency, the interrupt routine filters out most key releases.
 */
  int ch, make, escape;
  static int CAD_count = 0;
  static int rebooting = 0;

  /* Check for CTRL-ALT-DEL, and if found, halt the computer. This would
   * be better done in keyboard() in case TTY is hung, except control and
   * alt are set in the high level code.
   */
  if (ctrl && alt && (scode == DEL_SCAN || scode == INS_SCAN))
  {
	if (++CAD_count == 3) {
		cons_stop();
		sys_abort(RBT_DEFAULT);
	}
	sys_kill(INIT_PROC_NR, SIGABRT);
	rebooting = 1;
  }
  
   if(rebooting)
  	return -1;

  /* High-order bit set on key release. */
  make = (scode & KEY_RELEASE) == 0;		/* true if pressed */

  ch = map_key(scode &= ASCII_MASK);		/* map to ASCII */

  escape = esc;		/* Key is escaped?  (true if added since the XT) */
  esc = 0;

  switch (ch) {
  	case CTRL:		/* Left or right control key */
		*(escape ? &ctrl_r : &ctrl_l) = make;
		ctrl = ctrl_l | ctrl_r;
		break;
  	case SHIFT:		/* Left or right shift key */
		*(scode == RSHIFT_SCAN ? &shift_r : &shift_l) = make;
		shift = shift_l | shift_r;
		break;
  	case ALT:		/* Left or right alt key */
		*(escape ? &alt_r : &alt_l) = make;
		alt = alt_l | alt_r;
		if (sticky_alt_mode && (alt_r && (alt_down < make))) {
			locks[ccurrent] ^= ALT_LOCK;
		}
		alt_down = make;
		break;
  	case CALOCK:		/* Caps lock - toggle on 0 -> 1 transition */
		if (caps_down < make) {
			locks[ccurrent] ^= CAPS_LOCK;
			set_leds();
		}
		caps_down = make;
		break;
  	case NLOCK:		/* Num lock */
		if (num_down < make) {
			locks[ccurrent] ^= NUM_LOCK;
			set_leds();
		}
		num_down = make;
		break;
  	case SLOCK:		/* Scroll lock */
		if (scroll_down < make) {
			locks[ccurrent] ^= SCROLL_LOCK;
			set_leds();
		}
		scroll_down = make;
		break;
  	case EXTKEY:		/* Escape keycode */
		esc = 1;		/* Next key is escaped */
		return(-1);
  	default:		/* A normal key */
		if(!make)
			return -1;
		if(ch)
			return ch;
		{
			static char seen[2][NR_SCAN_CODES];
			int notseen = 0, ei;
			ei = escape ? 1 : 0;
			if(scode >= 0 && scode < NR_SCAN_CODES) {
				notseen = !seen[ei][scode];
				seen[ei][scode] = 1;
			} else {
				printf("tty: scode %d makes no sense\n", scode);
			}
			if(notseen) {
		  		printf("tty: ignoring unrecognized %s "
					"scancode 0x%x\n",
  				escape ? "escaped" : "straight", scode);
			}
		}
  		return -1;
  }

  /* Key release, or a shift type key. */
  return(-1);
}

/*===========================================================================*
 *				set_leds				     *
 *===========================================================================*/
PRIVATE void set_leds()
{
/* Set the LEDs on the caps, num, and scroll lock keys */
  int s;
  if (! machine.pc_at) return;	/* PC/XT doesn't have LEDs */

  kb_wait();			/* wait for buffer empty  */
  if ((s=sys_outb(KEYBD, LED_CODE)) != OK)
      printf("Warning, sys_outb couldn't prepare for LED values: %d\n", s);
   				/* prepare keyboard to accept LED values */
  kb_ack();			/* wait for ack response  */

  kb_wait();			/* wait for buffer empty  */
  if ((s=sys_outb(KEYBD, locks[ccurrent])) != OK)
      printf("Warning, sys_outb couldn't give LED values: %d\n", s);
				/* give keyboard LED values */
  kb_ack();			/* wait for ack response  */
}

/*===========================================================================*
 *				kbc_cmd0				     *
 *===========================================================================*/
PRIVATE void kbc_cmd0(cmd)
int cmd;
{
	kb_wait();
	if(sys_outb(KB_COMMAND, cmd) != OK)
		printf("kbc_cmd0: sys_outb failed\n");
}

/*===========================================================================*
 *				kbc_cmd1				     *
 *===========================================================================*/
PRIVATE void kbc_cmd1(cmd, data)
int cmd;
int data;
{
	kb_wait();
	if(sys_outb(KB_COMMAND, cmd) != OK)
		printf("kbc_cmd1: 1 sys_outb failed\n");
	kb_wait();
	if(sys_outb(KEYBD, data) != OK)
		printf("kbc_cmd1: 2 sys_outb failed\n");
}


/*===========================================================================*
*                              kbc_read                                     *
*===========================================================================*/
PRIVATE int kbc_read()
{
	int i;
	unsigned long byte, st;
#if 0
	struct micro_state ms;
#endif

#if DEBUG
	printf("in kbc_read\n");
#endif

	/* Wait at most 1 second for a byte from the keyboard or
	* the kbd controller, return -1 on a timeout.
	*/
	for (i= 0; i<1000000; i++)
 #if 0
	micro_start(&ms);
	do
#endif
	{
		if(sys_inb(KB_STATUS, &st) != OK)
			printf("kbc_read: 1 sys_inb failed\n");
		if (st & KB_OUT_FULL)
		{
			micro_delay(KBC_IN_DELAY);
			if(sys_inb(KEYBD, &byte) != OK)
				printf("kbc_read: 2 sys_inb failed\n");
			if (st & KB_AUX_BYTE)
				printf("kbc_read: aux byte 0x%x\n", byte);
#if DEBUG
			printf("keyboard`kbc_read: returning byte 0x%x\n",
				byte);
#endif
			return byte;
		}
	}
#if 0
	while (micro_elapsed(&ms) < 1000000);
#endif
	panic("kbc_read failed to complete");
	return EINVAL;
}


/*===========================================================================*
 *				kb_wait					     *
 *===========================================================================*/
PRIVATE int kb_wait()
{
/* Wait until the controller is ready; return zero if this times out. */

  int retries;
  unsigned long status;
  int s, isaux;
  unsigned char byte;

  retries = MAX_KB_BUSY_RETRIES + 1;	/* wait until not busy */
  do {
      s = sys_inb(KB_STATUS, &status);
      if(s != OK)
	printf("kb_wait: sys_inb failed: %d\n", s);
      if (status & KB_OUT_FULL) {
	  if (scan_keyboard(&byte, &isaux))
	  {
#if 0
		  printf("ignoring %sbyte in kb_wait\n", isaux ? "AUX " : "");
#endif
	  }
      }
      if (! (status & (KB_IN_FULL|KB_OUT_FULL)) )
          break;			/* wait until ready */
  } while (--retries != 0);		/* continue unless timeout */ 
  return(retries);		/* zero on timeout, positive if ready */
}

/*===========================================================================*
 *				kb_ack					     *
 *===========================================================================*/
PRIVATE int kb_ack()
{
/* Wait until kbd acknowledges last command; return zero if this times out. */

  int retries, s;
  unsigned long u8val;


  retries = MAX_KB_ACK_RETRIES + 1;
  do {
      s = sys_inb(KEYBD, &u8val);
	if(s != OK)
		printf("kb_ack: sys_inb failed: %d\n", s);
      if (u8val == KB_ACK)	
          break;		/* wait for ack */
  } while(--retries != 0);	/* continue unless timeout */

  return(retries);		/* nonzero if ack received */
}

/*===========================================================================*
 *				kb_init					     *
 *===========================================================================*/
PUBLIC void kb_init(tp)
tty_t *tp;
{
/* Initialize the keyboard driver. */

  tp->tty_devread = kb_read;	/* input function */
}

/*===========================================================================*
 *				kb_init_once				     *
 *===========================================================================*/
PUBLIC void kb_init_once(void)
{
  int i;
  u8_t ccb;

  env_parse("sticky_alt", "d", 0, &sticky_alt_mode, 0, 1);
  env_parse("debug_fkeys", "d", 0, &debug_fkeys, 0, 1);

  set_leds();			/* turn off numlock led */
  scan_keyboard(NULL, NULL);	/* discard leftover keystroke */

      /* Clear the function key observers array. Also see func_key(). */
      for (i=0; i<12; i++) {
          fkey_obs[i].proc_nr = NONE;	/* F1-F12 observers */
          fkey_obs[i].events = 0;	/* F1-F12 observers */
          sfkey_obs[i].proc_nr = NONE;	/* Shift F1-F12 observers */
          sfkey_obs[i].events = 0;	/* Shift F1-F12 observers */
      }

      kbd.minor= KBD_MINOR;
      kbdaux.minor= KBDAUX_MINOR;

      /* Set interrupt handler and enable keyboard IRQ. */
      irq_hook_id = KEYBOARD_IRQ;	/* id to be returned on interrupt */
      if ((i=sys_irqsetpolicy(KEYBOARD_IRQ, IRQ_REENABLE, &irq_hook_id)) != OK)
          panic("Couldn't set keyboard IRQ policy: %d", i);
      if ((i=sys_irqenable(&irq_hook_id)) != OK)
          panic("Couldn't enable keyboard IRQs: %d", i);
      kbd_irq_set |= (1 << KEYBOARD_IRQ);

      /* Set AUX interrupt handler and enable AUX IRQ. */
      aux_irq_hook_id = KBD_AUX_IRQ;	/* id to be returned on interrupt */
      if ((i=sys_irqsetpolicy(KBD_AUX_IRQ, IRQ_REENABLE,
		&aux_irq_hook_id)) != OK)
          panic("Couldn't set AUX IRQ policy: %d", i);
      if ((i=sys_irqenable(&aux_irq_hook_id)) != OK)
          panic("Couldn't enable AUX IRQs: %d", i);
      kbd_irq_set |= (1 << KBD_AUX_IRQ);

	/* Disable the keyboard and aux */
	kbc_cmd0(KBC_DI_KBD);
	kbc_cmd0(KBC_DI_AUX);

	/* Get the current configuration byte */
	kbc_cmd0(KBC_RD_RAM_CCB);
	ccb= kbc_read();

	/* Enable both interrupts. */
	kbc_cmd1(KBC_WR_RAM_CCB, ccb | 3);

	/* Re-enable the keyboard device. */
	kbc_cmd0(KBC_EN_KBD);

	/* Enable the aux device. */
	kbc_cmd0(KBC_EN_AUX);
}

/*===========================================================================*
 *				kbd_loadmap				     *
 *===========================================================================*/
PUBLIC int kbd_loadmap(m, safe)
message *m;
int safe;
{
/* Load a new keymap. */
  int result;
  if(safe) {
    result = sys_safecopyfrom(m->IO_ENDPT, (vir_bytes) m->ADDRESS,
  	0, (vir_bytes) keymap, (vir_bytes) sizeof(keymap), D);
  } else {
    result = sys_vircopy(m->IO_ENDPT, D, (vir_bytes) m->ADDRESS,
  	SELF, D, (vir_bytes) keymap, 
  	(vir_bytes) sizeof(keymap));
  }
  return(result);
}

/*===========================================================================*
 *				do_fkey_ctl				     *
 *===========================================================================*/
PUBLIC void do_fkey_ctl(m_ptr)
message *m_ptr;			/* pointer to the request message */
{
/* This procedure allows processes to register a function key to receive
 * notifications if it is pressed. At most one binding per key can exist.
 */
  int i; 
  int result = EINVAL;

  switch (m_ptr->FKEY_REQUEST) {	/* see what we must do */
  case FKEY_MAP:			/* request for new mapping */
      result = OK;			/* assume everything will be ok*/
      for (i=0; i < 12; i++) {		/* check F1-F12 keys */
          if (bit_isset(m_ptr->FKEY_FKEYS, i+1) ) {
#if DEAD_CODE
	/* Currently, we don't check if the slot is in use, so that IS
	 * can recover after a crash by overtaking its existing mappings.
	 * In future, a better solution will be implemented.
	 */
              if (fkey_obs[i].proc_nr == NONE) { 
#endif
    	          fkey_obs[i].proc_nr = m_ptr->m_source;
    	          fkey_obs[i].events = 0;
    	          bit_unset(m_ptr->FKEY_FKEYS, i+1);
#if DEAD_CODE
    	      } else {
    	          printf("WARNING, fkey_map failed F%d\n", i+1);
    	          result = EBUSY;	/* report failure, but try rest */
    	      }
#endif
    	  }
      }
      for (i=0; i < 12; i++) {		/* check Shift+F1-F12 keys */
          if (bit_isset(m_ptr->FKEY_SFKEYS, i+1) ) {
#if DEAD_CODE
              if (sfkey_obs[i].proc_nr == NONE) { 
#endif
    	          sfkey_obs[i].proc_nr = m_ptr->m_source;
    	          sfkey_obs[i].events = 0;
    	          bit_unset(m_ptr->FKEY_SFKEYS, i+1);
#if DEAD_CODE
    	      } else {
    	          printf("WARNING, fkey_map failed Shift F%d\n", i+1);
    	          result = EBUSY;	/* report failure but try rest */
    	      }
#endif
    	  }
      }
      break;
  case FKEY_UNMAP:
      result = OK;			/* assume everything will be ok*/
      for (i=0; i < 12; i++) {		/* check F1-F12 keys */
          if (bit_isset(m_ptr->FKEY_FKEYS, i+1) ) {
              if (fkey_obs[i].proc_nr == m_ptr->m_source) { 
    	          fkey_obs[i].proc_nr = NONE;
    	          fkey_obs[i].events = 0;
    	          bit_unset(m_ptr->FKEY_FKEYS, i+1);
    	      } else {
    	          result = EPERM;	/* report failure, but try rest */
    	      }
    	  }
      }
      for (i=0; i < 12; i++) {		/* check Shift+F1-F12 keys */
          if (bit_isset(m_ptr->FKEY_SFKEYS, i+1) ) {
              if (sfkey_obs[i].proc_nr == m_ptr->m_source) { 
    	          sfkey_obs[i].proc_nr = NONE;
    	          sfkey_obs[i].events = 0;
    	          bit_unset(m_ptr->FKEY_SFKEYS, i+1);
    	      } else {
    	          result = EPERM;	/* report failure, but try rest */
    	      }
    	  }
      }
      break;
  case FKEY_EVENTS:
      m_ptr->FKEY_FKEYS = m_ptr->FKEY_SFKEYS = 0;
      for (i=0; i < 12; i++) {		/* check (Shift+) F1-F12 keys */
          if (fkey_obs[i].proc_nr == m_ptr->m_source) {
              if (fkey_obs[i].events) { 
                  bit_set(m_ptr->FKEY_FKEYS, i+1);
                  fkey_obs[i].events = 0;
              }
          }
          if (sfkey_obs[i].proc_nr == m_ptr->m_source) {
              if (sfkey_obs[i].events) { 
                  bit_set(m_ptr->FKEY_SFKEYS, i+1);
                  sfkey_obs[i].events = 0;
              }
          }
      }
      break;
  }

  /* Almost done, return result to caller. */
  m_ptr->m_type = result;
  send(m_ptr->m_source, m_ptr);
}

/*===========================================================================*
 *				func_key				     *
 *===========================================================================*/
PRIVATE int func_key(scode)
int scode;			/* scan code for a function key */
{
/* This procedure traps function keys for debugging purposes. Observers of 
 * function keys are kept in a global array. If a subject (a key) is pressed
 * the observer is notified of the event. Initialization of the arrays is done
 * in kb_init, where NONE is set to indicate there is no interest in the key.
 * Returns FALSE on a key release or if the key is not observable.
 */
  int key;
  int proc_nr;

  /* Ignore key releases. If this is a key press, get full key code. */
  if (scode & KEY_RELEASE) return(FALSE);	/* key release */
  key = map_key(scode);		 		/* include modifiers */

  /* Key pressed, now see if there is an observer for the pressed key.
   *	       F1-F12	observers are in fkey_obs array. 
   *	SHIFT  F1-F12	observers are in sfkey_req array. 
   *	CTRL   F1-F12	reserved (see kb_read)
   *	ALT    F1-F12	reserved (see kb_read)
   * Other combinations are not in use. Note that Alt+Shift+F1-F12 is yet
   * defined in <minix/keymap.h>, and thus is easy for future extensions.
   */
  if (F1 <= key && key <= F12) {		/* F1-F12 */
      proc_nr = fkey_obs[key - F1].proc_nr;	
      fkey_obs[key - F1].events ++ ;	
  } else if (SF1 <= key && key <= SF12) {	/* Shift F2-F12 */
      proc_nr = sfkey_obs[key - SF1].proc_nr;	
      sfkey_obs[key - SF1].events ++;	
  }
  else {
      return(FALSE);				/* not observable */
  }

  /* See if an observer is registered and send it a message. */
  if (proc_nr != NONE) { 
      notify(proc_nr);
  }
  return(TRUE);
}

/*===========================================================================*
 *				show_key_mappings			     *
 *===========================================================================*/
PRIVATE void show_key_mappings()
{
    int i,s;
    struct proc proc;

    printf("\n");
    printf("System information.   Known function key mappings to request debug dumps:\n");
    printf("-------------------------------------------------------------------------\n");
    for (i=0; i<12; i++) {

      printf(" %sF%d: ", i+1<10? " ":"", i+1);
      if (fkey_obs[i].proc_nr != NONE) {
          if ((s = sys_getproc(&proc, fkey_obs[i].proc_nr))!=OK)
              printf("%-14.14s", "<unknown>");
          else
              printf("%-14.14s", proc.p_name);
      } else {
          printf("%-14.14s", "<none>");
      }

      printf("    %sShift-F%d: ", i+1<10? " ":"", i+1);
      if (sfkey_obs[i].proc_nr != NONE) {
          if ((s = sys_getproc(&proc, sfkey_obs[i].proc_nr))!=OK)
              printf("%-14.14s", "<unknown>");
          else
              printf("%-14.14s", proc.p_name);
      } else {
          printf("%-14.14s", "<none>");
      }
      printf("\n");
    }
    printf("\n");
    printf("Press one of the registered function keys to trigger a debug dump.\n");
    printf("\n");
}

/*===========================================================================*
 *				scan_keyboard				     *
 *===========================================================================*/
PRIVATE int scan_keyboard(bp, isauxp)
unsigned char *bp;
int *isauxp;
{
  unsigned long b, sb;

  if(sys_inb(KB_STATUS, &sb) != OK)
	printf("scan_keyboard: sys_inb failed\n");

  if (!(sb & KB_OUT_FULL))
  {
	if (kbdout.avail && !kbdout.expect_ack)
		kbd_send();
	return 0;
  }
  if(sys_inb(KEYBD, &b) != OK)
	printf("scan_keyboard: 2 sys_inb failed\n");
#if 0
  printf("got byte 0x%x from %s\n", b, (sb & KB_AUX_BYTE) ? "AUX" : "keyboard");
#endif
  if (!(sb & KB_AUX_BYTE) && b == KB_ACK && kbdout.expect_ack)
  {
#if 0
	printf("got ACK from keyboard\n");
#endif
	kbdout.expect_ack= 0;
	micro_delay(KBC_IN_DELAY);
	kbd_send();
	return 0;
  }
  if (bp)
  	*bp= b;
  if (isauxp)
  	*isauxp= !!(sb & KB_AUX_BYTE);
  if (kbdout.avail && !kbdout.expect_ack)
  {
	micro_delay(KBC_IN_DELAY);
	kbd_send();
  }
  return 1;
}

/*===========================================================================*
 *				kbd_watchdog 				     *
 *===========================================================================*/
PRIVATE void kbd_watchdog(tmrp)
timer_t *tmrp;
{
	int r;
	clock_t now;

	kbd_watchdog_set= 0;
	if (!kbdout.avail)
		return;	/* Watchdog is no longer needed */
	if (!kbd_alive)
	{
		printf("kbd_watchdog: should reset keyboard\n");
	}
	kbd_alive= 0;

	set_timer(&tmr_kbd_wd, system_hz, kbd_watchdog, 0);

	kbd_watchdog_set= 1;
}
