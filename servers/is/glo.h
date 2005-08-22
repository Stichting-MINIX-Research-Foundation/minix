/* Global variables. */

/* Parameters needed to keep diagnostics at IS. */
#define DIAG_BUF_SIZE 1024
extern char diag_buf[DIAG_BUF_SIZE];	/* buffer for messages */
extern int diag_next;			/* next index to be written */
extern int diag_size;			/* size of all messages */

/* The parameters of the call are kept here. */
extern message m_in;		/* the input message itself */
extern message m_out;		/* the output message used for reply */
extern int who;			/* caller's proc number */
extern int callnr;		/* system call number */
extern int dont_reply;		/* normally 0; set to 1 to inhibit reply */

