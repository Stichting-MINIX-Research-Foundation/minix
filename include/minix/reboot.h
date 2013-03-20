#ifndef _MINIX_REBOOT_H
#define _MINIX_REBOOT_H

/* How to exit the system. */
#define RBT_HALT	   0	/* shut down the system */
#define RBT_REBOOT	   1	/* reboot the system */
#define RBT_PANIC	   2	/* the system panics */
#define RBT_POWEROFF       3    /* power off, reset if not possible */
#define RBT_RESET	   4	/* hard reset the system */
#define RBT_DEFAULT	   5	/* perform the default action du jour */
#define RBT_INVALID	   6	/* first invalid reboot flag */

int reboot(int);

#endif /* _MINIX_REBOOT_H */
