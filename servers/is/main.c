/* System Information Service. 
 * This service handles the various debugging dumps, such as the process
 * table, so that these no longer directly touch kernel memory. Instead, the 
 * system task is asked to copy some table in local memory. 
 * 
 * Created:
 *   Apr 29, 2004	by Jorrit N. Herder
 */

#include "is.h"

/* Set debugging level to 0, 1, or 2 to see no, some, all debug output. */
#define DEBUG_LEVEL	1
#define DPRINTF		if (DEBUG_LEVEL > 0) printf

/* Allocate space for the global variables. */
message m_in;		/* the input message itself */
message m_out;		/* the output message used for reply */
int who;		/* caller's proc number */
int callnr;		/* system call number */

extern int errno;	/* error number set by system library */

/* Diagnostic messages buffer. */
char diag_buf[DIAG_BUF_SIZE];
int diag_size = 0;
int diag_next = 0;

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void get_work, (void)				);
FORWARD _PROTOTYPE(void reply, (int whom, int result)			);
FORWARD _PROTOTYPE(void signal_handler, (int sig)			);

/*===========================================================================*
 *                                  main                                     *
 *===========================================================================*/
PUBLIC void main(void)
{
/* This is the main routine of this service. The main loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
    int result;                 
    sigset_t sigset;

    /* Initialize the server, then go to work. */
    init_server();

    /* Main loop - get work and do it, forever. */         
    while (TRUE) {              

        /* Wait for incoming message, sets 'callnr' and 'who'. */
        get_work();

        switch (callnr) {
            case SYS_EVENT:
                sigset = (sigset_t) m_in.NOTIFY_ARG;
                if (sigismember(&sigset, SIGKMESS)) {
                    printf("IS proc SIGKMESS\n");
            	    result = do_new_kmess(&m_in);
            	} else if (sigismember(&sigset, SIGTERM)) {
                    printf("IS proc SIGTERM\n");
            	} else {
            	    report("IS","warning, got unknown signal", NO_NUM);
            	}
            	continue;
            case DIAGNOSTICS:
            	result = do_diagnostics(&m_in);
            	break;
            case FKEY_PRESSED:
            	result = do_fkey_pressed(&m_in);
            	break;
            default: {
            	printf("Warning, IS got unexpected request %d from %d\n",
            		m_in.m_type, m_in.m_source);
            	result = EINVAL;
  	   }
	}

	/* Finally send reply message, unless disabled. */
	if (result != EDONTREPLY) {
	    reply(who, result);
        }
    }
}


/*===========================================================================*
 *				 signal_handler                              *
 *===========================================================================*/
PRIVATE void signal_handler(sig)
int sig;					/* signal number */
{
/* Expect a SIGTERM signal when this server must shutdown. */
  if (sig == SIGTERM) {
  	printf("Shutting down IS server due to SIGTERM.\n");
  	exit(0);
  } else {
  	printf("IS got signal %d\n", sig);
  }
}


/*===========================================================================*
 *				 init_server                                 *
 *===========================================================================*/
PRIVATE void init_server()
{
/* Initialize the information service. */
  int fkeys, sfkeys;
  int i, s;
  struct sigaction sigact;

  /* Install signal handler. Ask PM to transform signal into message. */
  sigact.sa_handler = SIG_MESS;
  sigact.sa_mask = ~0;			/* block all other signals */
  sigact.sa_flags = 0;			/* default behaviour */
  if (sigaction(SIGTERM, &sigact, NULL) != OK) 
      report("IS","warning, sigaction() failed", errno);

  /* Set key mappings. IS takes all of F1-F12 and Shift+F1-F6 . */
  fkeys = sfkeys = 0;
  for (i=1; i<=12; i++) bit_set(fkeys, i);
  for (i=1; i<= 6; i++) bit_set(sfkeys, i);
  if ((s=fkey_map(&fkeys, &sfkeys)) != OK)
      report("IS", "warning, sendrec failed:", s);
}

/*===========================================================================*
 *				   get_work                                  *
 *===========================================================================*/
PRIVATE void get_work()
{
    int status = 0;
    status = receive(ANY, &m_in);   /* this blocks until message arrives */
    if (OK != status)
        panic("IS","failed to receive message!", status);
    who = m_in.m_source;        /* message arrived! set sender */
    callnr = m_in.m_type;       /* set function call number */
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(who, result)
int who;                           	/* destination */
int result;                           	/* report result to replyee */
{
    int send_status;
    m_out.m_type = result;  		/* build reply message */
    send_status = send(who, &m_out);    /* send the message */
    if (OK != send_status)
        panic("IS", "unable to send reply!", send_status);
}



