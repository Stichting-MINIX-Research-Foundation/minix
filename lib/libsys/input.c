#include "syslib.h"
#include <minix/sysutil.h>
/*****************************************************************************
 *             tty_inject_event                                              *
 *****************************************************************************/
int tty_inject_event(type, code, val)
int type;
int code;
int val;
{
	message msg;
	msg.INPUT_TYPE  = type;
	msg.INPUT_CODE  = code;
	msg.INPUT_VALUE = val;

	return send(TTY_PROC_NR, &msg);
}
