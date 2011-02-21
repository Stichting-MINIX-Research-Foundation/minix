#ifndef _SYS_REBOOT_H_
#define _SYS_REBOOT_H_

/* How to exit the system or stop a server process. */
#define RBT_HALT	   0	/* shutdown and return to monitor */
#define RBT_REBOOT	   1	/* reboot the system through the monitor */
#define RBT_PANIC	   2	/* a server panics */
#define RBT_MONITOR	   3	/* let the monitor do this */
#define RBT_RESET	   4	/* hard reset the system */
#define RBT_DEFAULT	   5	/* return to monitor, reset if not possible */
#define RBT_INVALID	   6	/* first invalid reboot flag */

#endif
