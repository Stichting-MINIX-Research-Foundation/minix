/* Keyboard unsupport for ARM. Just stubs. */
#include <minix/ipc.h>
#include <sys/termios.h>
#include "tty.h"

void
do_fkey_ctl(message *m)
{
}

void
do_input(message *m)
{
}

void
kb_init_once(void)
{
}

int
kbd_loadmap(endpoint_t endpt, cp_grant_id_t grant)
{
	return 0;
}

void
kb_init(tty_t *tp)
{
}
