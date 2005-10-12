/* Reincarnation Server.  This servers starts new system services and detects
 * they are exiting.   In case of errors, system services can be restarted.  
 * The RS server periodically checks the status of all registered services
 * services to see whether they are still alive.   The system services are 
 * expected to periodically send a heartbeat message. 
 * 
 * Created:
 *   Jul 22, 2005	by Jorrit N. Herder
 */
#include "rs.h"
#include <minix/dmap.h>
#include "../../kernel/const.h"
#include "../../kernel/type.h"

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m)				);
FORWARD _PROTOTYPE(void reply, (int whom, int result)			);
FORWARD _PROTOTYPE(int do_getsysinfo, (message *m)			);

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
	      continue;				/* no reply is expected */
          case SYS_SIG:
              sigset = (sigset_t) m.NOTIFY_ARG;
              if (sigismember(&sigset, SIGCHLD)) {
                  do_exit(&m);
              } 
              if (sigismember(&sigset, SIGTERM) ||
                  sigismember(&sigset, SIGKSTOP)) {
                  /* Prevent restarting services. */
		  do_shutdown(NULL);
              }
              continue;				/* no reply is expected */
	  default:				/* heartbeat notification */
	      printf("Got heartbeat from %d\n", who);
	      if (rproc_ptr[who] != NULL)	/* mark heartbeat time */ 
		  rproc_ptr[who]->r_alive_tm = m.NOTIFY_TIMESTAMP;
	  }
      }

      /* If this is not a notification message, it is a normal request. 
       * Handle the request and send a reply to the caller. 
       */
      else {	
          switch(call_nr) {
          case SRV_UP:
              result = do_start(&m);
              break;
          case SRV_DOWN:
              result = do_stop(&m);
              break;
          case SRV_SHUTDOWN:
              result = do_shutdown(&m);
              break;
          case GETSYSINFO:
	      printf("RS got GETSYSINFO request from %d\n", m.m_source);
              result = do_getsysinfo(&m);
              break;
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
  
  /* Change working directory to /sbin, where the binaries for the programs
   * in the system image are. 
   */
  chdir("/sbin/");
  for (s=0; s< NR_BOOT_PROCS; s++) {
      ip = &image[s];
      if (ip->proc_nr >= 0) {
          nr_in_use ++;
          rproc[s].r_flags = IN_USE;
          rproc[s].r_proc_nr = ip->proc_nr;
          rproc[s].r_pid = getnpid(ip->proc_nr);
	  for(t=0; t< NR_DEVICES; t++)
	      if (dmap[t].dmap_driver == ip->proc_nr)
                  rproc[s].r_dev_nr = t;
          strcpy(rproc[s].r_cmd, ip->proc_name);
          rproc[s].r_argc = 1;
          rproc[s].r_argv[0] = rproc[s].r_cmd;
          rproc[s].r_argv[1] = NULL;
      }
  }

  /* Set alarm to periodically check driver status. */
  if (OK != (s=sys_setalarm(HZ, 0)))
      panic("RS", "couldn't set alarm", s);

}


/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
PRIVATE int do_getsysinfo(m_ptr)
message *m_ptr;
{
  vir_bytes src_addr, dst_addr;
  int dst_proc;
  size_t len;
  int s;

  switch(m_ptr->m1_i1) {
  case SI_PROC_TAB:
  	src_addr = (vir_bytes) rproc;
  	len = sizeof(struct rproc) * NR_SYS_PROCS;
  	break; 
  default:
  	return(EINVAL);
  }

  dst_proc = m_ptr->m_source;
  dst_addr = (vir_bytes) m_ptr->m1_p1;
  if (OK != (s=sys_datacopy(SELF, src_addr, dst_proc, dst_addr, len)))
  	return(s);
  return(OK);
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



