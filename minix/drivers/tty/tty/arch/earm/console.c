/* Console unsupport for ARM. Just stubs. */
#include <minix/ipc.h>
#include <sys/termios.h>
#include "tty.h"

void
do_video(message *m, int ipc_status)
{
}

void
scr_init(tty_t *tp)
{
}

void
cons_stop(void)
{
}

void
beep_x(unsigned int freq, clock_t dur)
{
}

int
con_loadfont(endpoint_t endpt, cp_grant_id_t grant)
{
	return 0;
}
