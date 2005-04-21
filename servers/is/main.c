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
int is_proc_nr;		/* own process number */
message m_in;		/* the input message itself */
message m_out;		/* the output message used for reply */
int who;		/* caller's proc number */
int callnr;		/* system call number */

/* Diagnostic messages buffer. */
char diag_buf[DIAG_BUF_SIZE];
int diag_size = 0;
int diag_next = 0;

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void get_work, (void)				);
FORWARD _PROTOTYPE(void reply, (int whom, int result)			);


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

    /* Initialize the server, then go to work. */
    init_server();

    /* Main loop - get work and do it, forever. */         
    while (TRUE) {              

        /* Wait for incoming message, sets 'callnr' and 'who'. */
        get_work();

        switch (callnr) {
            case NEW_KMESS:
            	result = do_new_kmess(&m_in);
            	break;
            case DIAGNOSTICS:
            	result = do_diagnostics(&m_in);
            	break;
            case FKEY_PRESSED:
            	result = do_fkey_pressed(&m_in);
            	break;
            case HARD_STOP: 
            	sys_exit(0); 
            	/* never reached */
            	continue;
            default:
            	result = EINVAL;
	}

	/* Finally send reply message, unless disabled. */
	if (result != EDONTREPLY) {
	    reply(who, result);
        }
    }
}


/*===========================================================================*
 *				    report                                   *
 *===========================================================================*/
PUBLIC void report(mess, num)
char *mess;				/* message format to print */
int num;				/* number to go with the message */
{
  if (num != NO_NUM) {
      printf("IS: %s %d\n", mess, num);
  } else {
      printf("IS: %s\n", mess);
  }
}


/*===========================================================================*
 *				 init_server                                 *
 *===========================================================================*/
PRIVATE void init_server()
{
/* Initialize the information service. */
    message m;
    int r;
    long key;

    /* Set own process number. */
    is_proc_nr = IS_PROC_NR;

    /* Set key mappings. IS takes all of F2-F12. F1 is TTY reserved. */
    for (key=F1; key<=F12; key++) {
        if ((r=fkey_enable(key)) != OK) {
    	    printf("IS: WARNING: couldn't register F%d key: %d\n",
    	        (key-F1+1), r);
    	}
    }

    /* Display status message ... */
    printf("IS: information service is alive and kicking; press F1-F12 for dumps\n");
}

/*===========================================================================*
 *				   get_work                                  *
 *===========================================================================*/
PRIVATE void get_work()
{
    int status = 0;
    status = receive(ANY, &m_in);   /* this blocks until message arrives */
    if (OK != status)
        server_panic("IS","failed to receive message!", status);
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
        server_panic("IS", "unable to send reply!", send_status);
}



