/* Keyboard unsupport for ARM. Just stubs. */
#include <minix/ipc.h>
#include <termios.h>
#include "tty.h"

void
kbd_interrupt(message *m)
{
}

void
do_fkey_ctl(message *m)
{
}

void
do_kb_inject(message *m)
{
}

void
do_kbd(message *m)
{
}

void
do_kbdaux(message *m)
{
}

void
kb_init_once(void)
{
}

int
kbd_status(message *m)
{
	return 0;
}

int
kbd_loadmap(message *m)
{
	return 0;
}

void
kb_init(tty_t *tp)
{
}
