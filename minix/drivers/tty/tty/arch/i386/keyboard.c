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
#include <sys/reboot.h>
#include <sys/select.h>
#include <sys/termios.h>
#include <signal.h>
#include <machine/archtypes.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/input.h>
#include <minix/keymap.h>
#include <minix/ds.h>
#include <assert.h>
#include "tty.h"

static u16_t keymap[NR_SCAN_CODES][MAP_COLS] = {
#include "keymaps/us-std.src"
};

#define KB_IN_BYTES	  32	/* size of keyboard input buffer */

/* Scan codes in the input buffer are in the 0000h-00E7h range inclusive, plus
 * the following bit if the key was released rather than pressed.
 */
#define RELEASE_BIT	0x8000

static unsigned short inbuf[KB_IN_BYTES];
static unsigned short *inhead = inbuf;
static unsigned short *intail = inbuf;
static int incount;

static int alt_l;		/* left alt key state */
static int alt_r;		/* right alt key state */
static int alt;			/* either alt key */
static int ctrl_l;		/* left control key state */
static int ctrl_r;		/* right control key state */
static int ctrl;		/* either control key */
static int shift_l;		/* left shift key state */
static int shift_r;		/* right shift key state */
static int shift;		/* either shift key */
static int num_down;		/* num lock key depressed */
static int caps_down;		/* caps lock key depressed */
static int scroll_down;		/* scroll lock key depressed */
static int alt_down;	        /* alt key depressed */
static int locks[NR_CONS];	/* per console lock keys state */

/* Lock key active bits.  Chosen to be equal to the input LED mask bits. */
#define SCROLL_LOCK	(1 << INPUT_LED_SCROLLLOCK)
#define NUM_LOCK	(1 << INPUT_LED_NUMLOCK)
#define CAPS_LOCK	(1 << INPUT_LED_CAPSLOCK)
#define ALT_LOCK	0x10

static char numpad_map[12] =
		{'H', 'Y', 'A', 'B', 'D', 'C', 'V', 'U', 'G', 'S', 'T', '@'};

static char *fkey_map[12] =
		{"11", "12", "13", "14", "15", "17",	/* F1-F6 */
		 "18", "19", "20", "21", "23", "24"};	/* F7-F12 */

/* Variables and definition for observed function keys. */
typedef struct observer { endpoint_t proc_nr; int events; } obs_t;
static obs_t  fkey_obs[12];	/* observers for F1-F12 */
static obs_t sfkey_obs[12];	/* observers for SHIFT F1-F12 */

static endpoint_t input_endpt = NONE;

static long sticky_alt_mode = 0;
static long debug_fkeys = 1;

static int func_key(int scode);
static unsigned make_break(int scode);
static void set_leds(void);
static void show_key_mappings(void);
static unsigned map_key(int scode);

/*===========================================================================*
 *				map_key					     *
 *===========================================================================*/
static unsigned map_key(scode)
int scode;
{
/* Map a scan code to an ASCII code. */

  int caps, column, lk;
  u16_t *keyrow;

  keyrow = keymap[scode];

  caps = shift;
  lk = locks[ccurrent];
  if ((lk & NUM_LOCK) && (keyrow[0] & HASNUM)) caps = !caps;
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
  return keyrow[column] & ~(HASNUM | HASCAPS);
}

/*===========================================================================*
 *				do_input				     *
 *===========================================================================*/
void do_input(message *msg)
{
  unsigned short scode;
  endpoint_t endpt;
  int r;

  switch (msg->m_type) {
  case TTY_INPUT_UP:
	if ((r = ds_retrieve_label_endpt("input", &endpt)) != OK) {
		printf("TTY: unable to retrieve INPUT endpoint (%d)\n", r);
		return;
	}
	if (endpt != msg->m_source) {
		printf("TTY: up request from non-INPUT %u\n", msg->m_source);
		return;
	}

	input_endpt = msg->m_source;

	/* Pass the current state of the LEDs to INPUT. */
	set_leds();

	break;

  case TTY_INPUT_EVENT:
	if (msg->m_source != input_endpt) {
		printf("TTY: input event from non-INPUT %u\n", msg->m_source);
		return;
	}

	/* Only handle keyboard keys. */
	if (msg->m_input_tty_event.page != INPUT_PAGE_KEY)
		return;

	/* Only handle known USB HID keyboard codes (the 00h-E7h range). */
	scode = msg->m_input_tty_event.code;
	if (scode >= NR_SCAN_CODES)
		return;

	/* Is it a KEY RELEASE? */
	if (msg->m_input_tty_event.value == INPUT_RELEASE)
		scode |= RELEASE_BIT;

	if (incount < KB_IN_BYTES) {
		*inhead++ = scode;
		if (inhead == inbuf + KB_IN_BYTES) inhead = inbuf;
		incount++;
		tty_table[ccurrent].tty_events = 1;
	}

	break;

  default:
	panic("do_input called for unknown message type %x", msg->m_type);
  }
}

/*===========================================================================*
 *				kb_read					     *
 *===========================================================================*/
static int kb_read(tp, try)
tty_t *tp;
int try;
{
/* Process characters from the circular keyboard buffer. */
  char buf[7], *p, suffix;
  unsigned short scode;
  unsigned ch;
  
  /* always use the current console */
  tp = &tty_table[ccurrent];

  if (try)
	return (incount > 0);

  while (incount > 0) {
	/* Take one key scan code. */
	scode = *intail++;
	if (intail == inbuf + KB_IN_BYTES) intail = inbuf;
	incount--;

	/* Function keys are being used for debug dumps (if enabled). */
	if (debug_fkeys && func_key(scode)) continue;

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
		/* (CF1 <= ch && ch <= CF12) */ {
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
		(void) in_process(tp, buf, p - buf);
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
		case CF7: sigchar(line2tty(CONS_MINOR), SIGQUIT, 1); break;
		case CF8: sigchar(line2tty(CONS_MINOR), SIGINT, 1); break;
		case CF9: sigchar(line2tty(CONS_MINOR), SIGKILL, 1); break;
  	    }
	}
  }

  return 1;
}

/*===========================================================================*
 *				make_break				     *
 *===========================================================================*/
static unsigned make_break(int scode)
{
/* This routine can handle keyboards that interrupt only on key depression,
 * as well as keyboards that interrupt on key depression and key release.
 * For efficiency, the interrupt routine filters out most key releases.
 */
  int ch, make;
  static int CAD_count = 0;
  static int rebooting = 0;

  /* Check for CTRL-ALT-DEL, and if found, halt the computer. */
  if (ctrl && alt && (scode == INPUT_KEY_DELETE || scode == INPUT_KEY_INSERT))
  {
	if (++CAD_count == 3) {
		cons_stop();
		sys_abort(RB_AUTOBOOT);
	}
	sys_kill(INIT_PROC_NR, SIGABRT);
	rebooting = 1;
  }
  
  if (rebooting)
	return -1;

  /* High-order bit set on key release. */
  make = !(scode & RELEASE_BIT);	/* true if pressed */

  ch = map_key(scode &= ~RELEASE_BIT);	/* map to ASCII */

  switch (ch) {
	case LCTRL:		/* Left or right control key */
	case RCTRL:
		*(ch == RCTRL ? &ctrl_r : &ctrl_l) = make;
		ctrl = ctrl_l | ctrl_r;
		break;
	case LSHIFT:		/* Left or right shift key */
	case RSHIFT:
		*(ch == RSHIFT ? &shift_r : &shift_l) = make;
		shift = shift_l | shift_r;
		break;
	case LALT:		/* Left or right alt key */
	case RALT:
		*(ch == RALT ? &alt_r : &alt_l) = make;
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
  	default:		/* A normal key */
		if(!make)
			return -1;
		if(ch)
			return ch;
		/* Ignore unmapped key codes. */
  		return -1;
  }

  /* Key release, or a shift type key. */
  return(-1);
}

/*===========================================================================*
 *				set_leds				     *
 *===========================================================================*/
static void set_leds(void)
{
/* Make INPUT set the LEDs on the caps, num, and scroll lock keys. */
  message m;
  int r;

  if (input_endpt == NONE)
	return;

  memset(&m, 0, sizeof(m));

  m.m_type = INPUT_SETLEDS;
  m.m_input_linputdriver_setleds.led_mask = locks[ccurrent] & ~ALT_LOCK;

  if ((r = asynsend3(input_endpt, &m, AMF_NOREPLY)) != OK)
	printf("TTY: asynsend to INPUT %u failed (%d)\n", input_endpt, r);
}

/*===========================================================================*
 *				kb_init					     *
 *===========================================================================*/
void kb_init(tp)
tty_t *tp;
{
/* Initialize the keyboard driver. */

  tp->tty_devread = kb_read;	/* input function */
}

/*===========================================================================*
 *				kb_init_once				     *
 *===========================================================================*/
void kb_init_once(void)
{
  int i;

  env_parse("sticky_alt", "d", 0, &sticky_alt_mode, 0, 1);
  env_parse("debug_fkeys", "d", 0, &debug_fkeys, 0, 1);

  /* Clear the function key observers array. Also see func_key(). */
  for (i = 0; i < 12; i++) {
	fkey_obs[i].proc_nr = NONE;	/* F1-F12 observers */
	fkey_obs[i].events = 0;		/* F1-F12 observers */
	sfkey_obs[i].proc_nr = NONE;	/* Shift F1-F12 observers */
	sfkey_obs[i].events = 0;	/* F1-F12 observers */
  }
}

/*===========================================================================*
 *				kbd_loadmap				     *
 *===========================================================================*/
int kbd_loadmap(endpoint_t endpt, cp_grant_id_t grant)
{
/* Load a new keymap. */
  return sys_safecopyfrom(endpt, grant, 0, (vir_bytes) keymap, sizeof(keymap));
}

/*===========================================================================*
 *				do_fkey_ctl				     *
 *===========================================================================*/
void do_fkey_ctl(m_ptr)
message *m_ptr;			/* pointer to the request message */
{
/* This procedure allows processes to register a function key to receive
 * notifications if it is pressed. At most one binding per key can exist.
 */
  int s, i;
  int result = EINVAL;

  switch (m_ptr->m_lsys_tty_fkey_ctl.request) {	/* see what we must do */
  case FKEY_MAP:			/* request for new mapping */
      result = OK;			/* assume everything will be ok*/
      for (i=0; i < 12; i++) {		/* check F1-F12 keys */
          if (bit_isset(m_ptr->m_lsys_tty_fkey_ctl.fkeys, i+1) ) {
#if DEAD_CODE
	/* Currently, we don't check if the slot is in use, so that IS
	 * can recover after a crash by overtaking its existing mappings.
	 * In future, a better solution will be implemented.
	 */
              if (fkey_obs[i].proc_nr == NONE) { 
#endif
    	          fkey_obs[i].proc_nr = m_ptr->m_source;
    	          fkey_obs[i].events = 0;
    	          bit_unset(m_ptr->m_lsys_tty_fkey_ctl.fkeys, i+1);
#if DEAD_CODE
    	      } else {
    	          printf("WARNING, fkey_map failed F%d\n", i+1);
    	          result = EBUSY;	/* report failure, but try rest */
    	      }
#endif
    	  }
      }
      for (i=0; i < 12; i++) {		/* check Shift+F1-F12 keys */
          if (bit_isset(m_ptr->m_lsys_tty_fkey_ctl.sfkeys, i+1) ) {
#if DEAD_CODE
              if (sfkey_obs[i].proc_nr == NONE) { 
#endif
    	          sfkey_obs[i].proc_nr = m_ptr->m_source;
    	          sfkey_obs[i].events = 0;
    	          bit_unset(m_ptr->m_lsys_tty_fkey_ctl.sfkeys, i+1);
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
          if (bit_isset(m_ptr->m_lsys_tty_fkey_ctl.fkeys, i+1) ) {
              if (fkey_obs[i].proc_nr == m_ptr->m_source) { 
    	          fkey_obs[i].proc_nr = NONE;
    	          fkey_obs[i].events = 0;
    	          bit_unset(m_ptr->m_lsys_tty_fkey_ctl.fkeys, i+1);
    	      } else {
    	          result = EPERM;	/* report failure, but try rest */
    	      }
    	  }
      }
      for (i=0; i < 12; i++) {		/* check Shift+F1-F12 keys */
          if (bit_isset(m_ptr->m_lsys_tty_fkey_ctl.sfkeys, i+1) ) {
              if (sfkey_obs[i].proc_nr == m_ptr->m_source) { 
    	          sfkey_obs[i].proc_nr = NONE;
    	          sfkey_obs[i].events = 0;
    	          bit_unset(m_ptr->m_lsys_tty_fkey_ctl.sfkeys, i+1);
    	      } else {
    	          result = EPERM;	/* report failure, but try rest */
    	      }
    	  }
      }
      break;
  case FKEY_EVENTS:
      result = OK;			/* everything will be ok*/
      m_ptr->m_tty_lsys_fkey_ctl.fkeys = m_ptr->m_tty_lsys_fkey_ctl.sfkeys = 0;
      for (i=0; i < 12; i++) {		/* check (Shift+) F1-F12 keys */
          if (fkey_obs[i].proc_nr == m_ptr->m_source) {
              if (fkey_obs[i].events) { 
                  bit_set(m_ptr->m_tty_lsys_fkey_ctl.fkeys, i+1);
                  fkey_obs[i].events = 0;
              }
          }
          if (sfkey_obs[i].proc_nr == m_ptr->m_source) {
              if (sfkey_obs[i].events) { 
                  bit_set(m_ptr->m_tty_lsys_fkey_ctl.sfkeys, i+1);
                  sfkey_obs[i].events = 0;
              }
          }
      }
      break;
  }

  /* Almost done, return result to caller. */
  m_ptr->m_type = result;
  if ((s = ipc_sendnb(m_ptr->m_source, m_ptr)) != OK)
	printf("TTY: unable to reply to %d: %d", m_ptr->m_source, s);
}

/*===========================================================================*
 *				func_key				     *
 *===========================================================================*/
static int func_key(scode)
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
  if (scode & RELEASE_BIT) return(FALSE);	/* key release */
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
      ipc_notify(proc_nr);
  }
  return(TRUE);
}

/*===========================================================================*
 *				show_key_mappings			     *
 *===========================================================================*/
static void show_key_mappings()
{
    int i,s;

    printf("\n");
    printf("System information.   Known function key mappings to request debug dumps:\n");
    printf("-------------------------------------------------------------------------\n");
    for (i=0; i<12; i++) {

      printf(" %sF%d: ", i+1<10? " ":"", i+1);
      if (fkey_obs[i].proc_nr != NONE) {
          printf("%-14u", fkey_obs[i].proc_nr);
      } else {
          printf("%-14.14s", "<none>");
      }

      printf("    %sShift-F%d: ", i+1<10? " ":"", i+1);
      if (sfkey_obs[i].proc_nr != NONE) {
          printf("%-14u", sfkey_obs[i].proc_nr);
      } else {
          printf("%-14.14s", "<none>");
      }
      printf("\n");
    }
    printf("\n");
    printf("Press one of the registered function keys to trigger a debug dump.\n");
    printf("\n");
}
