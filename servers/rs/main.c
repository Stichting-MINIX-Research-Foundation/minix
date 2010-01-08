/* Reincarnation Server.  This servers starts new system services and detects
 * they are exiting.   In case of errors, system services can be restarted.  
 * The RS server periodically checks the status of all registered services
 * services to see whether they are still alive.   The system services are 
 * expected to periodically send a heartbeat message. 
 * 
 * Changes:
 *   Nov 22, 2009: rewrite of boot process (Cristiano Giuffrida)
 *   Jul 22, 2005: Created  (Jorrit N. Herder)
 */
#include "inc.h"
#include <fcntl.h>
#include <a.out.h>
#include <minix/crtso.h>
#include "../../kernel/const.h"
#include "../../kernel/type.h"
#include "../../kernel/proc.h"
#include "../pm/mproc.h"
#include "../pm/const.h"

/* Declare some local functions. */
FORWARD _PROTOTYPE(void exec_image_copy, ( int boot_proc_idx,
    struct boot_image *ip, struct rproc *rp)                            );
FORWARD _PROTOTYPE(void boot_image_info_lookup, ( endpoint_t endpoint,
    struct boot_image *image,
    struct boot_image **ip, struct boot_image_priv **pp,
    struct boot_image_sys **sp, struct boot_image_dev **dp)             );
FORWARD _PROTOTYPE(void catch_boot_init_ready, (endpoint_t endpoint)	);
FORWARD _PROTOTYPE(void sig_handler, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m)				);
FORWARD _PROTOTYPE(void reply, (int whom, message *m_out)		);

/* The buffer where the boot image is copied during initialization. */
PRIVATE int boot_image_buffer_size;
PRIVATE char *boot_image_buffer;

/* Flag set when memory unmapping can be done. */
EXTERN int unmap_ok;

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void)                      );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

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

  /* SEF local startup. */
  sef_local_startup();

  /* Main loop - get work and do it, forever. */         
  while (TRUE) {              

      /* Wait for request message. */
      get_work(&m);
      who_e = m.m_source;
      who_p = _ENDPOINT_P(who_e);
      if(who_p < -NR_TASKS || who_p >= NR_PROCS)
	panic("RS","message from bogus source", who_e);

      call_nr = m.m_type;

      /* Now determine what to do.  Four types of requests are expected:
       * - Heartbeat messages (notifications from registered system services)
       * - System notifications (POSIX signals or synchronous alarm)
       * - User requests (control messages to manage system services)
       * - Ready messages (reply messages from registered services)
       */

      /* Notification messages are control messages and do not need a reply.
       * These include heartbeat messages and system notifications.
       */
      if (is_notify(m.m_type)) {
          switch (who_p) {
          case CLOCK:
	      do_period(&m);			/* check services status */
	      continue;
          case PM_PROC_NR:                      /* signal or PM heartbeat */
	      sig_handler();				
	  default:				/* heartbeat notification */
	      if (rproc_ptr[who_p] != NULL) {	/* mark heartbeat time */ 
		  rproc_ptr[who_p]->r_alive_tm = m.NOTIFY_TIMESTAMP;
	      } else {
		  printf("Warning, RS got unexpected notify message from %d\n",
		      m.m_source);
	      }
	  }
      }

      /* If we get this far, this is a normal request.
       * Handle the request and send a reply to the caller. 
       */
      else {
	  if (call_nr != GETSYSINFO && 
	  	(call_nr < RS_RQ_BASE || call_nr >= RS_RQ_BASE+0x100))
	  {
		/* Ignore invalid requests. Do not try to reply. */
		printf("RS: got invalid request %d from endpoint %d\n",
			call_nr, m.m_source);
		continue;
	  }

          /* Handler functions are responsible for permission checking. */
          switch(call_nr) {
          /* User requests. */
	  case RS_UP:		result = do_up(&m);		break;
          case RS_DOWN: 	result = do_down(&m); 		break;
          case RS_REFRESH: 	result = do_refresh(&m); 	break;
          case RS_RESTART: 	result = do_restart(&m); 	break;
          case RS_SHUTDOWN: 	result = do_shutdown(&m); 	break;
          case RS_UPDATE: 	result = do_update(&m); 	break;
          case GETSYSINFO: 	result = do_getsysinfo(&m); 	break;
	  case RS_LOOKUP:	result = do_lookup(&m);		break;
	  /* Ready messages. */
	  case RS_INIT: 	result = do_init_ready(&m); 	break;
	  case RS_LU_PREPARE: 	result = do_upd_ready(&m); 	break;
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
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);     /* RS can only start fresh. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the reincarnation server. */
  struct sigaction sa;
  struct boot_image *ip;
  int s,i,j;
  int nr_image_srvs, nr_image_priv_srvs, nr_uncaught_init_srvs;
  struct rproc *rp;
  struct rprocpub *rpub;
  struct boot_image image[NR_BOOT_PROCS];
  struct mproc mproc[NR_PROCS];
  struct exec header;
  struct boot_image_priv *boot_image_priv;
  struct boot_image_sys *boot_image_sys;
  struct boot_image_dev *boot_image_dev;

  /* See if we run in verbose mode. */
  env_parse("rs_verbose", "d", 0, &rs_verbose, 0, 1);

  /* Initialize the global init descriptor. */
  rinit.rproctab_gid = cpf_grant_direct(ANY, (vir_bytes) rprocpub,
      sizeof(rprocpub), CPF_READ);
  if(!GRANT_VALID(rinit.rproctab_gid)) {
      panic("RS", "unable to create rprocpub table grant", rinit.rproctab_gid);
  }

  /* Initialize the global update descriptor. */
  rupdate.flags = 0;

  /* Get a copy of the boot image table. */
  if ((s = sys_getimage(image)) != OK) {
      panic("RS", "unable to get copy of boot image table", s);
  }

  /* Determine the number of system services in the boot image table and
   * compute the size required for the boot image buffer.
   */
  nr_image_srvs = 0;
  boot_image_buffer_size = 0;
  for(i=0;i<NR_BOOT_PROCS;i++) {
      ip = &image[i];

      /* System services only. */
      if(iskerneln(_ENDPOINT_P(ip->endpoint))) {
          continue;
      }
      nr_image_srvs++;

      /* Lookup the corresponding entry in the boot image sys table. */
      boot_image_info_lookup(ip->endpoint, image,
          NULL, NULL, &boot_image_sys, NULL);

      /* If we must keep a copy of this system service, read the header
       * and increase the size of the boot image buffer.
       */
      if(boot_image_sys->flags & SF_USE_COPY) {
          if((s = sys_getaoutheader(&header, i)) != OK) {
              panic("RS", "unable to get copy of a.out header", s);
          }
          boot_image_buffer_size += header.a_hdrlen
              + header.a_text + header.a_data;
      }
  }

  /* Determine the number of entries in the boot image priv table and make sure
   * it matches the number of system services in the boot image table.
   */
  nr_image_priv_srvs = 0;
  for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
      boot_image_priv = &boot_image_priv_table[i];

      /* System services only. */
      if(iskerneln(_ENDPOINT_P(boot_image_priv->endpoint))) {
          continue;
      }
      nr_image_priv_srvs++;
  }
  if(nr_image_srvs != nr_image_priv_srvs) {
      panic("RS", "boot image table and boot image priv table mismatch",
          NO_NUM);
  }

  /* Allocate boot image buffer. */
  if(boot_image_buffer_size > 0) {
      boot_image_buffer = rs_startup_sbrk(boot_image_buffer_size);
      if(boot_image_buffer == (char *) -1) {
          panic("RS", "unable to allocate boot image buffer", NO_NUM);
      }
  }

  /* Reset the system process table. */
  for (rp=BEG_RPROC_ADDR; rp<END_RPROC_ADDR; rp++) {
      rp->r_flags = 0;
      rp->r_pub = &rprocpub[rp - rproc];
      rp->r_pub->in_use = FALSE;
  }

  /* Initialize the system process table in 4 steps, each of them following
   * the appearance of system services in the boot image priv table.
   * - Step 1: get a copy of the executable image of every system service that
   * requires it while it is not yet running.
   * In addition, set priviliges, sys properties, and dev properties (if any)
   * for every system service.
   */
  for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
      boot_image_priv = &boot_image_priv_table[i];

      /* System services only. */
      if(iskerneln(_ENDPOINT_P(boot_image_priv->endpoint))) {
          continue;
      }

      /* Lookup the corresponding entries in other tables. */
      boot_image_info_lookup(boot_image_priv->endpoint, image,
          &ip, NULL, &boot_image_sys, &boot_image_dev);
      rp = &rproc[boot_image_priv - boot_image_priv_table];
      rpub = rp->r_pub;

      /*
       * Get a copy of the executable image if required.
       */
      rp->r_exec_len = 0;
      rp->r_exec = NULL;
      if(boot_image_sys->flags & SF_USE_COPY) {
          exec_image_copy(ip - image, ip, rp);
      }

      /*
       * Set privileges.
       */
      /* Get label. */
      strcpy(rpub->label, boot_image_priv->label);

      if(boot_image_priv->endpoint != RS_PROC_NR) {
          /* Force a static priv id for system services in the boot image. */
          rp->r_priv.s_id = static_priv_id(
              _ENDPOINT_P(boot_image_priv->endpoint));

          /* Initialize privilege bitmaps. */
          rp->r_priv.s_flags = boot_image_priv->flags;         /* priv flags */
          rp->r_priv.s_trap_mask = boot_image_priv->trap_mask; /* traps */
          memcpy(&rp->r_priv.s_ipc_to, &boot_image_priv->ipc_to,
                            sizeof(rp->r_priv.s_ipc_to));      /* targets */

          /* Initialize kernel call mask bitmap from unordered set. */
          fill_call_mask(boot_image_priv->k_calls, NR_SYS_CALLS,
              rp->r_priv.s_k_call_mask, KERNEL_CALL, TRUE);

          /* Set the privilege structure. */
          if ((s = sys_privctl(ip->endpoint, SYS_PRIV_SET_SYS, &(rp->r_priv)))
              != OK) {
              panic("RS", "unable to set privilege structure", s);
          }
      }

      /* Synch the privilege structure with the kernel. */
      if ((s = sys_getpriv(&(rp->r_priv), ip->endpoint)) != OK) {
          panic("RS", "unable to synch privilege structure", s);
      }

      /*
       * Set sys properties.
       */
      rpub->sys_flags = boot_image_sys->flags;        /* sys flags */

      /*
       * Set dev properties.
       */
      rpub->dev_nr = boot_image_dev->dev_nr;          /* major device number */
      rpub->dev_style = boot_image_dev->dev_style;    /* device style */
      rpub->period = boot_image_dev->period;          /* heartbeat period */

      /* Get process name. */
      strcpy(rpub->proc_name, ip->proc_name);

      /* Get command settings. */
      rp->r_cmd[0]= '\0';
      rp->r_argv[0] = rp->r_cmd;
      rp->r_argv[1] = NULL;
      rp->r_argc = 1;
      rp->r_script[0]= '\0';

      /* Initialize vm call mask bitmap from unordered set. */
      fill_call_mask(boot_image_priv->vm_calls, NR_VM_CALLS,
          rpub->vm_call_mask, VM_RQ_BASE, TRUE);

      /* Get some settings from the boot image table. */
      rp->r_nice = ip->priority;
      rpub->endpoint = ip->endpoint;

      /* Set some defaults. */
      rp->r_uid = 0;                           /* root */
      rp->r_check_tm = 0;                      /* not checked yet */
      getuptime(&rp->r_alive_tm);              /* currently alive */
      rp->r_stop_tm = 0;                       /* not exiting yet */
      rp->r_restarts = 0;                      /* no restarts so far */
      rp->r_set_resources = 0;                 /* don't set resources */

      /* Mark as in use. */
      rp->r_flags = RS_IN_USE;
      rproc_ptr[_ENDPOINT_P(rpub->endpoint)]= rp;
      rpub->in_use = TRUE;
  }

  /* - Step 2: allow every system service in the boot image to run.
   */
  nr_uncaught_init_srvs = 0;
  for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
      boot_image_priv = &boot_image_priv_table[i];

      /* System services only. */
      if(iskerneln(_ENDPOINT_P(boot_image_priv->endpoint))) {
          continue;
      }

      /* Ignore RS. */
      if(boot_image_priv->endpoint == RS_PROC_NR) {
          continue;
      }

      /* Lookup the corresponding slot in the system process table. */
      rp = &rproc[boot_image_priv - boot_image_priv_table];
      rpub = rp->r_pub;

      /* Allow the service to run. */
      if ((s = sys_privctl(rpub->endpoint, SYS_PRIV_ALLOW, NULL)) != OK) {
          panic("RS", "unable to initialize privileges", s);
      }

      /* Initialize service. We assume every service will always get
       * back to us here at boot time.
       */
      if(boot_image_priv->flags & SYS_PROC) {
          if ((s = init_service(rp, SEF_INIT_FRESH)) != OK) {
              panic("RS", "unable to initialize service", s);
          }
          if(rpub->sys_flags & SF_SYNCH_BOOT) {
              /* Catch init ready message now to synchronize. */
              catch_boot_init_ready(rpub->endpoint);
          }
          else {
              /* Catch init ready message later. */
              nr_uncaught_init_srvs++;
          }
      }
  }

  /* - Step 3: let every system service complete initialization by
   * catching all the init ready messages left.
   */
  while(nr_uncaught_init_srvs) {
      catch_boot_init_ready(ANY);
      nr_uncaught_init_srvs--;
  }

  /* - Step 4: all the system services in the boot image are now running.
   * Complete the initialization of the system process table in collaboration
   * with other system processes.
   */
  if ((s = getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc)) != OK) {
      panic("RS", "unable to get copy of PM process table", s);
  }
  for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
      boot_image_priv = &boot_image_priv_table[i];

      /* System services only. */
      if(iskerneln(_ENDPOINT_P(boot_image_priv->endpoint))) {
          continue;
      }

      /* Lookup the corresponding slot in the system process table. */
      rp = &rproc[boot_image_priv - boot_image_priv_table];
      rpub = rp->r_pub;

      /* Get pid from PM process table. */
      rp->r_pid = NO_PID;
      for (j = 0; j < NR_PROCS; j++) {
          if (mproc[j].mp_endpoint == rpub->endpoint) {
              rp->r_pid = mproc[j].mp_pid;
              break;
          }
      }
      if(j == NR_PROCS) {
          panic("RS", "unable to get pid", NO_NUM);
      }
  }

  /*
   * Now complete RS initialization process in collaboration with other
   * system services.
   */
  /* Let the rest of the system know about our dynamically allocated buffer. */
  if(boot_image_buffer_size > 0) {
      boot_image_buffer = rs_startup_sbrk_synch(boot_image_buffer_size);
      if(boot_image_buffer == (char *) -1) {
          panic("RS", "unable to synch boot image buffer", NO_NUM);
      }
  }

  /* Set alarm to periodically check service status. */
  if (OK != (s=sys_setalarm(RS_DELTA_T, 0)))
      panic("RS", "couldn't set alarm", s);

  /* Install signal handlers. Ask PM to transform signal into message. */
  sa.sa_handler = SIG_MESS;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGCHLD,&sa,NULL)<0) panic("RS","sigaction failed", errno);
  if (sigaction(SIGTERM,&sa,NULL)<0) panic("RS","sigaction failed", errno);

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

 /* Map out our own text and data. This is normally done in crtso.o
  * but RS is an exception - we don't get to talk to VM so early on.
  * That's why we override munmap() and munmap_text() in utility.c.
  *
  * _minix_unmapzero() is the same code in crtso.o that normally does
  * it on startup. It's best that it's there as crtso.o knows exactly
  * what the ranges are of the filler data.
  */
  unmap_ok = 1;
  _minix_unmapzero();

  return(OK);
}

/*===========================================================================*
 *	                    exec_image_copy                                  *
 *===========================================================================*/
PRIVATE void exec_image_copy(boot_proc_idx, ip, rp)
int boot_proc_idx;
struct boot_image *ip;
struct rproc *rp;
{
/* Copy the executable image of the given boot process. */
  int s;
  struct exec header;
  static char *boot_image_ptr = NULL;

  if(boot_image_ptr == NULL) {
      boot_image_ptr = boot_image_buffer;
  }
  s = NO_NUM;

  /* Get a.out header. */
  if(boot_image_buffer+boot_image_buffer_size - boot_image_ptr < sizeof(header)
      || (s = sys_getaoutheader(&header, boot_proc_idx)) != OK) {
      panic("RS", "unable to get copy of a.out header", s);
  }
  memcpy(boot_image_ptr, &header, header.a_hdrlen);
  boot_image_ptr += header.a_hdrlen;

  /* Get text segment. */
  if(boot_image_buffer+boot_image_buffer_size - boot_image_ptr < header.a_text
      || (s = rs_startup_segcopy(ip->endpoint, T, D, (vir_bytes) boot_image_ptr,
      header.a_text)) != OK) {
      panic("RS", "unable to get copy of text segment", s);
  }
  boot_image_ptr += header.a_text;

  /* Get data segment. */
  if(boot_image_buffer+boot_image_buffer_size - boot_image_ptr < header.a_data
      || (s = rs_startup_segcopy(ip->endpoint, D, D, (vir_bytes) boot_image_ptr,
      header.a_data)) != OK) {
      panic("RS", "unable to get copy of data segment", s);
  }
  boot_image_ptr += header.a_data;

  /* Set the executable image for the given boot process. */
  rp->r_exec_len = header.a_hdrlen + header.a_text + header.a_data;
  rp->r_exec = boot_image_ptr - rp->r_exec_len;
}

/*===========================================================================*
 *                         boot_image_info_lookup                            *
 *===========================================================================*/
PRIVATE void boot_image_info_lookup(endpoint, image, ip, pp, sp, dp)
endpoint_t endpoint;
struct boot_image *image;
struct boot_image **ip;
struct boot_image_priv **pp;
struct boot_image_sys **sp;
struct boot_image_dev **dp;
{
/* Lookup entries in boot image tables. */
  int i;

  /* When requested, locate the corresponding entry in the boot image table
   * or panic if not found.
   */
  if(ip) {
      for (i=0; i < NR_BOOT_PROCS; i++) {
          if(image[i].endpoint == endpoint) {
              *ip = &image[i];
              break;
          }
      }
      if(i == NR_BOOT_PROCS) {
          panic("RS", "boot image table lookup failed", NO_NUM);
      }
  }

  /* When requested, locate the corresponding entry in the boot image priv table
   * or panic if not found.
   */
  if(pp) {
      for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
          if(boot_image_priv_table[i].endpoint == endpoint) {
              *pp = &boot_image_priv_table[i];
              break;
          }
      }
      if(i == NULL_BOOT_NR) {
          panic("RS", "boot image priv table lookup failed", NO_NUM);
      }
  }

  /* When requested, locate the corresponding entry in the boot image sys table
   * or resort to the default entry if not found.
   */
  if(sp) {
      for (i=0; boot_image_sys_table[i].endpoint != DEFAULT_BOOT_NR; i++) {
          if(boot_image_sys_table[i].endpoint == endpoint) {
              *sp = &boot_image_sys_table[i];
              break;
          }
      }
      if(boot_image_sys_table[i].endpoint == DEFAULT_BOOT_NR) {
          *sp = &boot_image_sys_table[i];         /* accept the default entry */
      }
  }

  /* When requested, locate the corresponding entry in the boot image dev table
   * or resort to the default entry if not found.
   */
  if(dp) {
      for (i=0; boot_image_dev_table[i].endpoint != DEFAULT_BOOT_NR; i++) {
          if(boot_image_dev_table[i].endpoint == endpoint) {
              *dp = &boot_image_dev_table[i];
              break;
          }
      }
      if(boot_image_dev_table[i].endpoint == DEFAULT_BOOT_NR) {
          *dp = &boot_image_dev_table[i];         /* accept the default entry */
      }
  }
}

/*===========================================================================*
 *			      catch_boot_init_ready                          *
 *===========================================================================*/
PRIVATE void catch_boot_init_ready(endpoint)
endpoint_t endpoint;
{
/* Block and catch an init ready message from the given source. */
  int r;
  message m;
  struct rproc *rp;
  int result;

  /* Receive init ready message. */
  if ((r = receive(endpoint, &m)) != OK) {
      panic("RS", "unable to receive init reply", r);
  }
  if(m.m_type != RS_INIT) {
      panic("RS", "unexpected reply from service", m.m_source);
  }
  result = m.RS_INIT_RESULT;
  rp = rproc_ptr[_ENDPOINT_P(m.m_source)];

  /* Check result. */
  if(result != OK) {
      panic("RS", "unable to complete init for service", m.m_source);
  }

  /* Mark the slot as no longer initializing. */
  rp->r_flags &= ~RS_INITIALIZING;
  rp->r_check_tm = 0;
  getuptime(&rp->r_alive_tm);
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
    if (OK != (s=sef_receive(ANY, m_in))) 	/* wait for message */
        panic("RS", "sef_receive failed", s);
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

