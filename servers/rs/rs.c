/* Reincarnation Server.  This servers starts new system services and detects
 * they are exiting.  In case of errors, system services can be restarted.  
 * 
 * Created:
 *   Jul 22, 2005	by Jorrit N. Herder
 */

#include "rs.h"

/* Set debugging level to 0, 1, or 2 to see no, some, all debug output. */
#define DEBUG_LEVEL	1
#define DPRINTF		if (DEBUG_LEVEL > 0) printf

/* Allocate space for the global variables. */
message m_in;		/* the input message itself */
message m_out;		/* the output message used for reply */
int who;		/* caller's proc number */
int callnr;		/* system call number */

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void get_work, (void)				);
FORWARD _PROTOTYPE(void reply, (int whom, int result)			);

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(void)
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
      case SYS_SIG:
          /* Signals are passed by means of a notification message from SYSTEM. 
           * Extract the map of pending signals from the notification argument.
           */ 
          sigset = (sigset_t) m_in.NOTIFY_ARG;
  
          if (sigismember(&sigset, SIGCHLD)) {
                /* A child of this server exited. Take action. */    
                do_exit(&m_in);
          } 
          if (sigismember(&sigset, SIGUSR1)) {
                do_start(&m_in);
          } 
          if (sigismember(&sigset, SIGTERM)) {
                /* Nothing to do on shutdown. */    
          } 
          if (sigismember(&sigset, SIGKSTOP)) {
                /* Nothing to do on shutdown. */    
          }
          continue;
      case SRV_UP:
          result = do_start(&m_in);
          break;
      case SRV_DOWN:
          result = do_stop(&m_in);
          break;
      default: 
          printf("Warning, RS got unexpected request %d from %d\n",
            	m_in.m_type, m_in.m_source);
          result = EINVAL;
      }

      /* Finally send reply message, unless disabled. */
      if (result != EDONTREPLY) {
          reply(who, result);
      }
  }
}


/*===========================================================================*
 *				init_server                                  *
 *===========================================================================*/
PRIVATE void init_server(void)
{
/* Initialize the reincarnation server. */
  struct sigaction sa;

  /* Install signal handlers. Ask PM to transform signal into message. */
  sa.sa_handler = SIG_MESS;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGCHLD, &sa, NULL)<0) panic("RS","sigaction failed", errno);
  if (sigaction(SIGTERM, &sa, NULL)<0) panic("RS","sigaction failed", errno);
  if (sigaction(SIGABRT, &sa, NULL)<0) panic("RS","sigaction failed", errno);
  if (sigaction(SIGHUP,  &sa, NULL)<0) panic("RS","sigaction failed", errno);
}


/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
PRIVATE void get_work()
{
    int status = 0;
    status = receive(ANY, &m_in);   /* this blocks until message arrives */
    if (OK != status)
        panic("RS","failed to receive message!", status);
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
        panic("RS", "unable to send reply!", send_status);
}



