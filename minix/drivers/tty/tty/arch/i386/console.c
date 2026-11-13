/* Code and data for the IBM console driver.
 *
 * The 6845 video controller used by the IBM PC shares its video memory with
 * the CPU somewhere in the 0xB0000 memory bank.  To the 6845 this memory
 * consists of 16-bit words.  Each word has a character code in the low byte
 * and a so-called attribute byte in the high byte.  The CPU directly modifies
 * video memory to display characters, and sets two registers on the 6845 that
 * specify the video origin and the cursor position.  The video origin is the
 * place in video memory where the first character (upper left corner) can
 * be found.  Moving the origin is a fast way to scroll the screen.  Some
 * video adapters wrap around the top of video memory, so the origin can
 * move without bounds.  For other adapters screen memory must sometimes be
 * moved to reset the origin.  All computations on video memory use character
 * (word) addresses for simplicity and assume there is no wrapping.  The
 * assembly support functions translate the word addresses to byte addresses
 * and the scrolling function worries about wrapping.
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


extern cons_sw_t bios_cons_sw;
extern cons_sw_t fb_cons_sw;
extern cons_sw_t ser_cons_sw;

static cons_sw_t *cons_sw;

static int video_open(devminor_t minor, int access, endpoint_t user_endpt);
static int video_close(devminor_t minor);
static int video_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int flags,
	endpoint_t user_endpt, cdev_id_t id);

static struct chardriver video_tab = {
  .cdr_open	= video_open,
  .cdr_close	= video_close,
  .cdr_ioctl	= video_ioctl
};

/*===========================================================================*
 *				video_open				     *
 *===========================================================================*/
static int video_open(devminor_t minor, int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
  /* Should grant IOPL */
	if (cons_sw != NULL)
		cons_sw->sw_disable_console();
  return OK;
}

/*===========================================================================*
 *				video_close				     *
 *===========================================================================*/
static int video_close(devminor_t minor)
{
	if (cons_sw != NULL)
		cons_sw->sw_reenable_console();
  return OK;
}

/*===========================================================================*
 *				video_ioctl				     *
 *===========================================================================*/
static int video_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int flags,
	endpoint_t user_endpt, cdev_id_t id)
{
  return ENOTTY;
}

/*===========================================================================*
 *				do_video				     *
 *===========================================================================*/
void do_video(message *m, int ipc_status)
{
  chardriver_process(&video_tab, m, ipc_status);
}

/*===========================================================================*
 *				scr_init				     *
 *===========================================================================*/
void scr_init(tty_t *tp)
{
/* Initialize the screen driver. */

  int s;
  struct kinfo kinfo;		/* kernel information */
  
	if (OK != (s=sys_getkinfo(&kinfo))) {
        panic("Couldn't get kernel information: %d", s);
    }

	switch (kinfo.boot_mode) {
	case 0: // BIOS cinsole
		cons_sw = &bios_cons_sw;
		break;
	case 1: // UEFI fb console
		cons_sw = &fb_cons_sw;
		break;
	default:
		cons_sw = &ser_cons_sw;
	}
	
	cons_sw->sw_init(tp);
}

/*===========================================================================*
 *				cons_stop				     *
 *===========================================================================*/
void cons_stop(void)
{
/* Prepare for halt or reboot. */
	if (cons_sw != NULL)
		cons_sw->sw_stop();
}


/*===========================================================================*
 *				select_console				     *
 *===========================================================================*/
void select_console(int cons_line)
{
/* Set the current console to console number 'cons_line'. */
	if (cons_sw != NULL)
		cons_sw->sw_select_console(cons_line);
}

/*===========================================================================*
 *				con_loadfont				     *
 *===========================================================================*/
int con_loadfont(endpoint_t endpt, cp_grant_id_t grant)
{
  int r = !OK;
  
	if (cons_sw != NULL)
		r = cons_sw->sw_loadfont(endpt, grant);

  return(r);
}

