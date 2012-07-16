/* Global variables. */

#include <minix/param.h>

/* Parameters needed to keep diagnostics at IS. */
#define DIAG_BUF_SIZE 1024
extern char diag_buf[DIAG_BUF_SIZE];	/* buffer for messages */
extern int diag_next;			/* next index to be written */
extern int diag_size;			/* size of all messages */

/* Flag to indicate system-wide panic. */
extern int sys_panic;		/* if set, shutdown can be done */

/* The parameters of the call are kept here. */
extern int dont_reply;		/* normally 0; set to 1 to inhibit reply */

