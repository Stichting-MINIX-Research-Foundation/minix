/* Reincarnation Server.  This servers starts new system services and detects
 * they are exiting.   In case of errors, system services can be restarted.  
 * The RS server periodically checks the status of all registered services
 * services to see whether they are still alive.   The system services are 
 * expected to periodically send a heartbeat message. 
 * 
 * Created:
 *   Jul 22, 2005	by Jorrit N. Herder
 */
#include "inc.h"
#include <fcntl.h>
#include <minix/endpoint.h>
#include "../../kernel/const.h"
#include "../../kernel/type.h"

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void sig_handler, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m)				);
FORWARD _PROTOTYPE(void reply, (int whom, message *m_out)		);

/* Data buffers to retrieve info during initialization. */
PRIVATE struct boot_image image[NR_BOOT_PROCS];

long rs_verbose = 0;

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(void)
{
/* This is the main routine of this service. The main loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  message m;					/* request message */
  int call_nr, who_e,who_p;			/* call number and caller */
  int result;                 			/* result to return */
  sigset_t sigset;				/* system signal set */
  int s;
  uid_t euid;

  /* Initialize the server, then go to work. */
  init_server();	

  /* Main loop - get work and do it, forever. */         
  while (TRUE) {              

      /* Wait for request message. */
      get_work(&m);
      who_e = m.m_source;
      who_p = _ENDPOINT_P(who_e);
      if(who_p < -NR_TASKS || who_p >= NR_PROCS)
	panic("RS","message from bogus source", who_e);

      call_nr = m.m_type;

      /* Now determine what to do.  Three types of requests are expected: 
       * - Heartbeat messages (notifications from registered system services)
       * - System notifications (POSIX signals or synchronous alarm)
       * - User requests (control messages to manage system services)
       */

      /* Notification messages are control messages and do not need a reply.
       * These include heartbeat messages and system notifications.
       */
      if (m.m_type & NOTIFY_MESSAGE) {
          switch (call_nr) {
          case SYN_ALARM:
	      do_period(&m);			/* check drivers status */
	      continue;				
          case PROC_EVENT:
	      sig_handler();
              continue;				
	  default:				/* heartbeat notification */
	      if (rproc_ptr[who_p] != NULL)	/* mark heartbeat time */ 
		  rproc_ptr[who_p]->r_alive_tm = m.NOTIFY_TIMESTAMP;
	  }
      }

      /* If this is not a notification message, it is a normal request. 
       * Handle the request and send a reply to the caller. 
       */
      else {
	  if (call_nr < RS_RQ_BASE || call_nr >= RS_RQ_BASE+0x100)
	  {
		/* Ignore invalid requests. Do not try to reply. */
		printf("RS: got invalid request %d from endpoint %d\n",
			call_nr, m.m_source);
		continue;
	  }

	  /* Only root can make calls to rs */
	  euid= getpeuid(m.m_source);
	  if (euid != 0)
	  {
		printf("RS: got unauthorized request %d from endpoint %d\n",
			call_nr, m.m_source);
		m.m_type = EPERM;
		reply(who_e, &m);
		continue;
	  }

          switch(call_nr) {
          case RS_UP: 		result = do_up(&m, FALSE, 0); break;
          case RS_UP_COPY:	result = do_up(&m, TRUE, 0); break;
	  case RS_START:	result = do_start(&m);		break;
          case RS_DOWN: 	result = do_down(&m); 		break;
          case RS_REFRESH: 	result = do_refresh(&m); 	break;
          case RS_RESTART: 	result = do_restart(&m); 	break;
          case RS_SHUTDOWN: 	result = do_shutdown(&m); 	break;
          case GETSYSINFO: 	result = do_getsysinfo(&m); 	break;
          default: 
              printf("Warning, RS got unexpected request %d from %d\n",
                  m.m_type, m.m_source);
              result = EINVAL;
          }

          /* Finally send reply message, unless disabled. */
          if (result != EDONTREPLY) {
	      m.m_type = result;
              reply(who_e, &m);
          }
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
  struct boot_image *ip;
  int s,t;

  /* Install signal handlers. Ask PM to transform signal into message. */
  sa.sa_handler = SIG_MESS;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGCHLD,&sa,NULL)<0) panic("RS","sigaction failed", errno);
  if (sigaction(SIGTERM,&sa,NULL)<0) panic("RS","sigaction failed", errno);

  /* Initialize the system process table. Use the boot image from the kernel
   * and the device map from the FS to gather all needed information.
   */
  if ((s = sys_getimage(image)) != OK) 
      panic("RS","warning: couldn't get copy of image table", s);
  
  /* Set alarm to periodically check driver status. */
  if (OK != (s=sys_setalarm(RS_DELTA_T, 0)))
      panic("RS", "couldn't set alarm", s);

  /* See if we run in verbose mode. */
  env_parse("rs_verbose", "d", 0, &rs_verbose, 0, 1); 

  /* Initialize the exec pipe. */
  if (pipe(exec_pipe) == -1)
	panic("RS", "pipe failed", errno);
  if (fcntl(exec_pipe[0], F_SETFD,
	fcntl(exec_pipe[0], F_GETFD) | FD_CLOEXEC) == -1)
  {
	panic("RS", "fcntl set FD_CLOEXEC on pipe input failed", errno);
  }
  if (fcntl(exec_pipe[1], F_SETFD,
	fcntl(exec_pipe[1], F_GETFD) | FD_CLOEXEC) == -1)
  {
	panic("RS", "fcntl set FD_CLOEXEC on pipe output failed", errno);
  }
  if (fcntl(exec_pipe[0], F_SETFL,
	fcntl(exec_pipe[0], F_GETFL) | O_NONBLOCK) == -1)
  {
	panic("RS", "fcntl set O_NONBLOCK on pipe input failed", errno);
  }
}

/*===========================================================================*
 *				sig_handler                                  *
 *===========================================================================*/
PRIVATE void sig_handler()
{
  sigset_t sigset;
  int sig;

  /* Try to obtain signal set from PM. */
  if (getsigset(&sigset) != 0) return;

  /* Check for known signals. */
  if (sigismember(&sigset, SIGCHLD)) do_exit(NULL);
  if (sigismember(&sigset, SIGTERM)) do_shutdown(NULL);
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
PRIVATE void get_work(m_in)
message *m_in;				/* pointer to message */
{
    int s;				/* receive status */
    if (OK != (s=receive(ANY, m_in))) 	/* wait for message */
        panic("RS","receive failed", s);
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(who, m_out)
int who;                           	/* replyee */
message *m_out;                         /* reply message */
{
    int s;				/* send status */

    s = sendnb(who, m_out);		/* send the message */
    if (s != OK)
        printf("RS: unable to send reply to %d: %d\n", who, s);
}




