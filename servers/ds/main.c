/* Data Store Server. 
 * This service implements a little publish/subscribe data store that is 
 * crucial for the system's fault tolerance. Components that require state
 * can store it here, for later retrieval, e.g., after a crash and subsequent
 * restart by the reincarnation server. 
 * 
 * Created:
 *   Oct 19, 2005	by Jorrit N. Herder
 */

#include "inc.h"	/* include master header file */

/* Allocate space for the global variables. */
int who_e;		/* caller's proc number */
int callnr;		/* system call number */
int sys_panic;		/* flag to indicate system-wide panic */

extern int errno;	/* error number set by system library */

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (int argc, char **argv)		);
FORWARD _PROTOTYPE(void exit_server, (void)				);
FORWARD _PROTOTYPE(void sig_handler, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m_ptr)			);
FORWARD _PROTOTYPE(void reply, (int whom, message *m_ptr)		);

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char **argv)
{
/* This is the main routine of this service. The main loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  message m;
  int result;                 
  sigset_t sigset;

  /* Initialize the server, then go to work. */
  init_server(argc, argv);

  /* Main loop - get work and do it, forever. */         
  while (TRUE) {              

      /* Wait for incoming message, sets 'callnr' and 'who'. */
      get_work(&m);

      switch (callnr) {
      case PROC_EVENT:
	  sig_handler();
          continue;
      case DS_PUBLISH:
          result = do_publish(&m);
          break;
      case DS_RETRIEVE:
	  result = do_retrieve(&m);
	  break;
      case DS_SUBSCRIBE:
	  result = do_subscribe(&m);
	  break;
      case GETSYSINFO:
	  result = do_getsysinfo(&m);
	  break;
      default: 
          report("DS","warning, got illegal request from:", m.m_source);
          result = EINVAL;
      }

      /* Finally send reply message, unless disabled. */
      if (result != EDONTREPLY) {
          m.m_type = result;  		/* build reply message */
	  reply(who_e, &m);		/* send it away */
      }
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *				 init_server                                 *
 *===========================================================================*/
PRIVATE void init_server(int argc, char **argv)
{
/* Initialize the data store server. */
  int i, s;
  struct sigaction sigact;

  /* Install signal handler. Ask PM to transform signal into message. */
  sigact.sa_handler = SIG_MESS;
  sigact.sa_mask = ~0;			/* block all other signals */
  sigact.sa_flags = 0;			/* default behaviour */
  if (sigaction(SIGTERM, &sigact, NULL) < 0) 
      report("DS","warning, sigaction() failed", errno);
}

/*===========================================================================*
 *				 sig_handler                                 *
 *===========================================================================*/
PRIVATE void sig_handler()
{
/* Signal handler. */
  sigset_t sigset;
  int sig;

  /* Try to obtain signal set from PM. */
  if (getsigset(&sigset) != 0) return;

  /* Check for known signals. */
  if (sigismember(&sigset, SIGTERM)) {
      exit_server();
  }
}

/*===========================================================================*
 *				exit_server                                  *
 *===========================================================================*/
PRIVATE void exit_server()
{
/* Shut down the information service. */

  /* Done. Now exit. */
  exit(0);
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
PRIVATE void get_work(m_ptr)
message *m_ptr;				/* message buffer */
{
    int status = 0;
    status = receive(ANY, m_ptr);   /* this blocks until message arrives */
    if (OK != status)
        panic("DS","failed to receive message!", status);
    who_e = m_ptr->m_source;        /* message arrived! set sender */
    callnr = m_ptr->m_type;       /* set function call number */
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(who_e, m_ptr)
int who_e;                           	/* destination */
message *m_ptr;				/* message buffer */
{
    int s;
    s = send(who_e, m_ptr);    /* send the message */
    if (OK != s)
        panic("DS", "unable to send reply!", s);
}

