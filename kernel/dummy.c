/* A dummy task. Used to safely remove old tasks, and get notices if they get
 * called by accident (i.e., because of some bug...).
 * 
 * Created:
 *   Jul 27, 2004   by Jorrit N. Herder
 */

#include "kernel.h" 
#include "proc.h" 
#include <minix/com.h>

/* Allocated space for the global variables. */
message m_in;		/* the input message itself */
message m_out;		/* the output message used for reply */
int who;		/* caller's proc number */
int callnr;		/* system call number */

/* Declare some local functions. */
FORWARD _PROTOTYPE(void get_work, (void)				);
FORWARD _PROTOTYPE(void reply, (int whom, int result)			);


/*===========================================================================*
 *                             dummy_task                                    *
 *===========================================================================*/
PUBLIC void dummy_task()
{
/* This is the main routine of this service. In principle this should block
 * forever on getting new work - the dummy task is not supposed to be called.
 * If new work is received, somehow, diagnostics are printed.  
 */
    int result;                 

    /* kprintf("DUMMY: do nothing kernel task started (proc. nr %d).\n",
    	proc_number(proc_ptr)); */

    /* Main loop - get work and do it, forever. */         
    while (TRUE) {              

        /* Wait for incoming message, sets 'callnr' and 'who'. */
        get_work();

        /* There is work to do! (re)set some variables first. */
        result = EINVAL;	/* illegal send to dummy task */

	/* Print diagnostics: this was not supposed to happen. */
	kprintf("Dummy task: request received from %d.\n", who);

	/* Finally send reply message, unless disabled. */
	if (result != EDONTREPLY) {
	    reply(who, result);
        }
    }
}


/*===========================================================================*
 *				   get_work                                  *
 *===========================================================================*/
PRIVATE void get_work()
{
    int status = 0;
    status = receive(ANY, &m_in);   /* this blocks until message arrives */
    if (OK != status)
        kprintf("Dummy task failed to receive message: %d", status);
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
        kprintf("Dummy task unable to send reply: %d", send_status);
}



