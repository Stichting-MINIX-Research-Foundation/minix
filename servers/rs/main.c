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
#include <minix/dmap.h>
#include "../../kernel/const.h"
#include "../../kernel/type.h"

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m)				);
FORWARD _PROTOTYPE(void reply, (int whom, int result)			);

/* Data buffers to retrieve info during initialization. */
PRIVATE struct boot_image image[NR_BOOT_PROCS];
PUBLIC struct dmap dmap[NR_DEVICES];

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
  int call_nr, who;				/* call number and caller */
  int result;                 			/* result to return */
  sigset_t sigset;				/* system signal set */
  int s;

  /* Initialize the server, then go to work. */
  init_server();	

  /* Main loop - get work and do it, forever. */         
  while (TRUE) {              

      /* Wait for request message. */
      get_work(&m);
      who = m.m_source;
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
          case SYS_SIG:
              sigset = (sigset_t) m.NOTIFY_ARG;	/* check signals passed */
              if (sigismember(&sigset, SIGCHLD)) do_exit(&m);
              if (sigismember(&sigset, SIGTERM)) do_shutdown(NULL);
              if (sigismember(&sigset, SIGKSTOP)) do_shutdown(NULL);
              continue;				
	  default:				/* heartbeat notification */
	      if (rproc_ptr[who] != NULL)	/* mark heartbeat time */ 
		  rproc_ptr[who]->r_alive_tm = m.NOTIFY_TIMESTAMP;
	  }
      }

      /* If this is not a notification message, it is a normal request. 
       * Handle the request and send a reply to the caller. 
       */
      else {	
          switch(call_nr) {
          case RS_UP: 		result = do_up(&m); 		break;
          case RS_DOWN: 	result = do_down(&m); 		break;
          case RS_REFRESH: 	result = do_refresh(&m); 	break;
          case RS_RESCUE: 	result = do_rescue(&m); 	break;
          case RS_SHUTDOWN: 	result = do_shutdown(&m); 	break;
          case GETSYSINFO: 	result = do_getsysinfo(&m); 	break;
          default: 
              printf("Warning, RS got unexpected request %d from %d\n",
                  m.m_type, m.m_source);
              result = EINVAL;
          }

          /* Finally send reply message, unless disabled. */
          if (result != EDONTREPLY) {
              reply(who, result);
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
  if (sigaction(SIGABRT,&sa,NULL)<0) panic("RS","sigaction failed", errno);
  if (sigaction(SIGHUP, &sa,NULL)<0) panic("RS","sigaction failed", errno);

  /* Initialize the system process table. Use the boot image from the kernel
   * and the device map from the FS to gather all needed information.
   */
  if ((s = sys_getimage(image)) != OK) 
      panic("RS","warning: couldn't get copy of image table", s);
  if ((s = getsysinfo(FS_PROC_NR, SI_DMAP_TAB, dmap)) < 0)
      panic("RS","warning: couldn't get copy of dmap table", errno);
  
  /* Now initialize the table with the processes in the system image. 
   * Prepend /sbin/ to the binaries so that we can actually find them. 
   */
  for (s=0; s< NR_BOOT_PROCS; s++) {
      ip = &image[s];
      if (ip->proc_nr >= 0) {
          nr_in_use ++;
          rproc[s].r_flags = RS_IN_USE;
          rproc[s].r_proc_nr = ip->proc_nr;
          rproc[s].r_pid = getnpid(ip->proc_nr);
	  for(t=0; t< NR_DEVICES; t++)
	      if (dmap[t].dmap_driver == ip->proc_nr)
                  rproc[s].r_dev_nr = t;
	  strcpy(rproc[s].r_cmd, "/sbin/");
          strcpy(rproc[s].r_cmd+6, ip->proc_name);
          rproc[s].r_argc = 1;
          rproc[s].r_argv[0] = rproc[s].r_cmd;
          rproc[s].r_argv[1] = NULL;
      }
  }

  /* Set alarm to periodically check driver status. */
  if (OK != (s=sys_setalarm(RS_DELTA_T, 0)))
      panic("RS", "couldn't set alarm", s);

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
PRIVATE void reply(who, result)
int who;                           	/* replyee */
int result;                           	/* report result */
{
    message m_out;			/* reply message */
    int s;				/* send status */

    m_out.m_type = result;  		/* build reply message */
    if (OK != (s=send(who, &m_out)))    /* send the message */
        panic("RS", "unable to send reply", s);
}



