/* Keyboard driver for PC's and AT's.
 *
 * Changes: 
 *   Jul 13, 2004   processes can observe function keys  (Jorrit N. Herder)
 *   Jun 15, 2004   removed wreboot(), except panic dumps (Jorrit N. Herder)
 *   Feb 04, 1994   loadable keymaps  (Marcus Hampel)
 */

#include "../drivers.h"
#include <sys/time.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include "tty.h"
#include "keymaps/us-std.src"
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"

int irq_hook_id = -1;

/* Standard and AT keyboard.  (PS/2 MCA implies AT throughout.) */
#define KEYBD		0x60	/* I/O port for keyboard data */

/* AT keyboard. */
#define KB_COMMAND	0x64	/* I/O port for commands on AT */
#define KB_STATUS	0x64	/* I/O port for status on AT */
#define KB_ACK		0xFA	/* keyboard ack response */
#define KB_OUT_FULL	0x01	/* status bit set when keypress char pending */
#define KB_IN_FULL	0x02	/* status bit set when not ready to receive */
#define LED_CODE	0xED	/* command to keyboard to set LEDs */
#define MAX_KB_ACK_RETRIES 0x1000	/* max #times to wait for kb ack */
#define MAX_KB_BUSY_RETRIES 0x1000	/* max #times to loop while kb busy */
#define KBIT		0x80	/* bit used to ack characters to keyboard */

/* Miscellaneous. */
#define ESC_SCAN	0x01	/* reboot key when panicking */
#define SLASH_SCAN	0x35	/* to recognize numeric slash */
#define RSHIFT_SCAN	0x36	/* to distinguish left and right shift */
#define HOME_SCAN	0x47	/* first key on the numeric keypad */
#define INS_SCAN	0x52	/* INS for use in CTRL-ALT-INS reboot */
#define DEL_SCAN	0x53	/* DEL for use in CTRL-ALT-DEL reboot */

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
PRIVATE int locks[NR_CONS];	/* per console lock keys state */

/* Lock key active bits.  Chosen to be equal to the keyboard LED bits. */
#define SCROLL_LOCK	0x01
#define NUM_LOCK	0x02
#define CAPS_LOCK	0x04

PRIVATE char numpad_map[] =
		{'H', 'Y', 'A', 'B', 'D', 'C', 'V', 'U', 'G', 'S', 'T', '@'};

/* Variables and definition for observed function keys. */
typedef struct observer { int proc_nr; int events; } obs_t;
PRIVATE obs_t  fkey_obs[12];	/* observers for F1-F12 */
PRIVATE obs_t sfkey_obs[12];	/* observers for SHIFT F1-F12 */

FORWARD _PROTOTYPE( int kb_ack, (void) 					);
FORWARD _PROTOTYPE( int kb_wait, (void)				 	);
FORWARD _PROTOTYPE( int func_key, (int scode) 				);
FORWARD _PROTOTYPE( int scan_keyboard, (void) 				);
FORWARD _PROTOTYPE( unsigned make_break, (int scode) 			);
FORWARD _PROTOTYPE( void set_leds, (void) 				);
FORWARD _PROTOTYPE( void show_key_mappings, (void) 			);
FORWARD _PROTOTYPE( int kb_read, (struct tty *tp, int try) 		);
FORWARD _PROTOTYPE( unsigned map_key, (int scode) 			);

/*===========================================================================*
 *				map_key0				     *
 *===========================================================================*/
/* Map a scan code to an ASCII code ignoring modifiers. */
#define map_key0(scode)	 \
	((unsigned) keymap[(scode) * MAP_COLS])

/*===========================================================================*
 *				map_key					     *
 *===========================================================================*/
PRIVATE unsigned map_key(scode)
int scode;
{
/* Map a scan code to an ASCII code. */

  int caps, column, lk;
  u16_t *keyrow;

  if (scode == SLASH_SCAN && esc) return '/';	/* don't map numeric slash */

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
	column = 0;
	if (caps) column = 1;
	if (ctrl) column = 5;
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
  int scode;
  static timer_t timer;		/* timer must be static! */

  /* Fetch the character from the keyboard hardware and acknowledge it. */
  scode = scan_keyboard();

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
  char buf[3];
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

	/* Function keys are being used for debug dumps. */
	if (func_key(scode)) continue;

	/* Perform make/break processing. */
	ch = make_break(scode);

	if (ch <= 0xFF) {
		/* A normal character. */
		buf[0] = ch;
		(void) in_process(tp, buf, 1);
	} else
	if (HOME <= ch && ch <= INSRT) {
		/* An ASCII escape sequence generated by the numeric pad. */
		buf[0] = ESC;
		buf[1] = '[';
		buf[2] = numpad_map[ch - HOME];
		(void) in_process(tp, buf, 3);
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
  		case CF7: sigchar(&tty_table[CONSOLE], SIGQUIT); break;
  		case CF8: sigchar(&tty_table[CONSOLE], SIGINT); break;
  		case CF9: sigchar(&tty_table[CONSOLE], SIGKILL); break;
  	    }
	}
  }

  return 1;
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

  /* Check for CTRL-ALT-DEL, and if found, halt the computer. This would
   * be better done in keyboard() in case TTY is hung, except control and
   * alt are set in the high level code.
   */
  if (ctrl && alt && (scode == DEL_SCAN || scode == INS_SCAN))
  {
	if (++CAD_count == 3) sys_abort(RBT_HALT);
	sys_kill(INIT_PROC_NR, SIGABRT);
	return -1;
  }

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
		if (make) return(ch);
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
 *				kb_wait					     *
 *===========================================================================*/
PRIVATE int kb_wait()
{
/* Wait until the controller is ready; return zero if this times out. */

  int retries, status, temp;
  int s;

  retries = MAX_KB_BUSY_RETRIES + 1;	/* wait until not busy */
  do {
      s = sys_inb(KB_STATUS, &status);
      if (status & KB_OUT_FULL) {
          s = sys_inb(KEYBD, &temp);	/* discard value */
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
  u8_t u8val;

  retries = MAX_KB_ACK_RETRIES + 1;
  do {
      s = sys_inb(KEYBD, &u8val);
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

  set_leds();			/* turn off numlock led */
  scan_keyboard();		/* discard leftover keystroke */

      /* Clear the function key observers array. Also see func_key(). */
      for (i=0; i<12; i++) {
          fkey_obs[i].proc_nr = NONE;	/* F1-F12 observers */
          fkey_obs[i].events = 0;	/* F1-F12 observers */
          sfkey_obs[i].proc_nr = NONE;	/* Shift F1-F12 observers */
          sfkey_obs[i].events = 0;	/* Shift F1-F12 observers */
      }

      /* Set interrupt handler and enable keyboard IRQ. */
      irq_hook_id = KEYBOARD_IRQ;	/* id to be returned on interrupt */
      if ((i=sys_irqsetpolicy(KEYBOARD_IRQ, IRQ_REENABLE, &irq_hook_id)) != OK)
          panic("TTY",  "Couldn't set keyboard IRQ policy", i);
      if ((i=sys_irqenable(&irq_hook_id)) != OK)
          panic("TTY", "Couldn't enable keyboard IRQs", i);
      kbd_irq_set |= (1 << KEYBOARD_IRQ);
}

/*===========================================================================*
 *				kbd_loadmap				     *
 *===========================================================================*/
PUBLIC int kbd_loadmap(m)
message *m;
{
/* Load a new keymap. */
  int result;
  result = sys_vircopy(m->PROC_NR, D, (vir_bytes) m->ADDRESS,
  	SELF, D, (vir_bytes) keymap, 
  	(vir_bytes) sizeof(keymap));
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
  int result;

  switch (m_ptr->FKEY_REQUEST) {	/* see what we must do */
  case FKEY_MAP:			/* request for new mapping */
      result = OK;			/* assume everything will be ok*/
      for (i=0; i < 12; i++) {		/* check F1-F12 keys */
          if (bit_isset(m_ptr->FKEY_FKEYS, i+1) ) {
              if (fkey_obs[i].proc_nr == NONE) { 
    	          fkey_obs[i].proc_nr = m_ptr->m_source;
    	          fkey_obs[i].events = 0;
    	          bit_unset(m_ptr->FKEY_FKEYS, i+1);
    	      } else {
    	          printf("WARNING, fkey_map failed F%d\n", i+1);
    	          result = EBUSY;	/* report failure, but try rest */
    	      }
    	  }
      }
      for (i=0; i < 12; i++) {		/* check Shift+F1-F12 keys */
          if (bit_isset(m_ptr->FKEY_SFKEYS, i+1) ) {
              if (sfkey_obs[i].proc_nr == NONE) { 
    	          sfkey_obs[i].proc_nr = m_ptr->m_source;
    	          sfkey_obs[i].events = 0;
    	          bit_unset(m_ptr->FKEY_SFKEYS, i+1);
    	      } else {
    	          printf("WARNING, fkey_map failed Shift F%d\n", i+1);
    	          result = EBUSY;	/* report failure but try rest */
    	      }
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
  default:
          result =  EINVAL;		/* key cannot be observed */
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
  message m;
  int key;
  int proc_nr;
  int i,s;

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
      m.NOTIFY_TYPE = FKEY_PRESSED;
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
          if ((s=sys_getproc(&proc, fkey_obs[i].proc_nr))!=OK)
              printf("sys_getproc: %d\n", s);
          printf("%-14.14s", proc.p_name);
      } else {
          printf("%-14.14s", "<none>");
      }

      printf("    %sShift-F%d: ", i+1<10? " ":"", i+1);
      if (sfkey_obs[i].proc_nr != NONE) {
          if ((s=sys_getproc(&proc, sfkey_obs[i].proc_nr))!=OK)
              printf("sys_getproc: %d\n", s);
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
PRIVATE int scan_keyboard()
{
/* Fetch the character from the keyboard hardware and acknowledge it. */
  pvb_pair_t byte_in[2], byte_out[2];
  
  byte_in[0].port = KEYBD;	/* get the scan code for the key struck */
  byte_in[1].port = PORT_B;	/* strobe the keyboard to ack the char */
  sys_vinb(byte_in, 2);		/* request actual input */

  pv_set(byte_out[0], PORT_B, byte_in[1].value | KBIT); /* strobe bit high */
  pv_set(byte_out[1], PORT_B, byte_in[1].value);	/* then strobe low */
  sys_voutb(byte_out, 2);	/* request actual output */

  return(byte_in[0].value);		/* return scan code */
}

/*===========================================================================*
 *				do_panic_dumps 				     *
 *===========================================================================*/
PUBLIC void do_panic_dumps(m)
message *m;			/* request message to TTY */
{
/* Wait for keystrokes for printing debugging info and reboot. */
  int quiet, code;

  /* A panic! Allow debug dumps until user wants to shutdown. */
  printf("\nHit ESC to reboot, DEL to shutdown, F-keys for debug dumps\n");

  (void) scan_keyboard();	/* ack any old input */
  quiet = scan_keyboard();/* quiescent value (0 on PC, last code on AT)*/
  for (;;) {
	tickdelay(10);
	/* See if there are pending request for output, but don't block. 
	 * Diagnostics can span multiple printf()s, so do it in a loop.
	 */
	while (nb_receive(ANY, m) == OK) {
		switch(m->m_type) {
		case FKEY_CONTROL: do_fkey_ctl(m);      break;
		case SYS_SIG:	   do_new_kmess(m);	break;
		case DIAGNOSTICS:  do_diagnostics(m);	break;
		default:	;	/* do nothing */ 
		}
		tickdelay(1);		/* allow more */
	}
	code = scan_keyboard();
	if (code != quiet) {
		/* A key has been pressed. */
		switch (code) {			/* possibly abort MINIX */
		case ESC_SCAN:  sys_abort(RBT_REBOOT); 	return;	
		case DEL_SCAN:  sys_abort(RBT_HALT); 	return;	
		}
		(void) func_key(code);	     	/* check for function key */
		quiet = scan_keyboard();
	}
  }
}

