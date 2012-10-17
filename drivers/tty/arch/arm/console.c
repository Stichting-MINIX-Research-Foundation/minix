/* Console unsupport for ARM. Just stubs. */
#include <minix/ipc.h>
#include <termios.h>
#include "tty.h"

void
do_video(message *m)
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

int
con_loadfont(message *m)
{
	return 0;
}

