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
FORWARD _PROTOTYPE(void fill_call_mask, ( int *calls, int tot_nr_calls,
    bitchunk_t *call_mask, int call_base)                               );
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void sig_handler, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m)				);
FORWARD _PROTOTYPE(void reply, (int whom, message *m_out)		);

/* The buffer where the boot image is copied during initialization. */
PRIVATE int boot_image_buffer_size;
PRIVATE char *boot_image_buffer;

/* Macro to identify a system service in the boot image. This rules out
 * kernel tasks and the root system process (RS).
 */
#define isbootsrvprocn(n) (!iskerneln((n)) && !isrootsysn((n)))

/* Flag set when memory unmapping can be done. */
EXTERN int unmap_ok;

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
      if (is_notify(m.m_type)) {
          switch (who_p) {
          case CLOCK:
	      do_period(&m);			/* check drivers status */
	      continue;				
          case PM_PROC_NR:
	      sig_handler();
              continue;				
	  default:				/* heartbeat notification */
	      if (rproc_ptr[who_p] != NULL) {	/* mark heartbeat time */ 
		  rproc_ptr[who_p]->r_alive_tm = m.NOTIFY_TIMESTAMP;
	      } else {
		  printf("Warning, RS got unexpected notify message from %d\n",
		      m.m_source);
	      }
	  }
      }

      /* If this is not a notification message, it is a normal request. 
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
          case RS_UP: 		result = do_up(&m, FALSE, 0); break;
          case RS_UP_COPY:	result = do_up(&m, TRUE, 0); break;
	  case RS_START:	result = do_start(&m);		break;
          case RS_DOWN: 	result = do_down(&m); 		break;
          case RS_REFRESH: 	result = do_refresh(&m); 	break;
          case RS_RESTART: 	result = do_restart(&m); 	break;
          case RS_SHUTDOWN: 	result = do_shutdown(&m); 	break;
          case GETSYSINFO: 	result = do_getsysinfo(&m); 	break;
	  case RS_LOOKUP:	result = do_lookup(&m);		break;
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
 *			      fill_call_mask                                 *
 *===========================================================================*/
PRIVATE void fill_call_mask(calls, tot_nr_calls, call_mask, call_base)
int *calls;                     /* the unordered set of calls */
int tot_nr_calls;               /* the total number of calls */
bitchunk_t *call_mask;          /* the call mask to fill in */
int call_base;                  /* the base offset for the calls */
{
/* Fill a call mask from an unordered set of calls. */
  int i;
  bitchunk_t fv;
  int call_mask_size, nr_calls;

  call_mask_size = BITMAP_CHUNKS(tot_nr_calls);

  /* Count the number of calls to fill in. */
  nr_calls = 0;
  for(i=0; calls[i] != SYS_NULL_C; i++) {
      nr_calls++;
  }

  /* See if all calls are allowed and call mask must be completely filled. */
  fv = 0;
  if(nr_calls == 1 && calls[0] == SYS_ALL_C) {
      fv = (~0);
  }

  /* Fill or clear call mask. */
  for(i=0; i < call_mask_size; i++) {
      call_mask[i] = fv;
  }

  /* Not all calls allowed? Enter calls bit by bit. */
  if(!fv) {
      for(i=0; i < nr_calls; i++) {
          SET_BIT(call_mask, calls[i] - call_base);
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
  int s,i,j;
  int nr_image_srvs, nr_image_priv_srvs;
  struct rproc *rp;
  struct boot_image image[NR_BOOT_PROCS];
  struct mproc mproc[NR_PROCS];
  struct exec header;
  struct boot_image_priv *boot_image_priv;
  struct boot_image_sys *boot_image_sys;
  struct boot_image_dev *boot_image_dev;

  /* See if we run in verbose mode. */
  env_parse("rs_verbose", "d", 0, &rs_verbose, 0, 1);

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
      if(!isbootsrvprocn(_ENDPOINT_P(ip->endpoint))) {
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
      if(!isbootsrvprocn(_ENDPOINT_P(boot_image_priv->endpoint))) {
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

  /* Initialize the system process table in 3 steps, each of them following
   * the appearance of system services in the boot image priv table.
   * - Step 1: get a copy of the executable image of every system service that
   * requires it while it is not yet running.
   * In addition, set priviliges, sys properties, and dev properties (if any)
   * for every system service.
   */
  for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
      boot_image_priv = &boot_image_priv_table[i];

      /* System services only. */
      if(!isbootsrvprocn(_ENDPOINT_P(boot_image_priv->endpoint))) {
          continue;
      }

      /* Lookup the corresponding entries in other tables. */
      boot_image_info_lookup(boot_image_priv->endpoint, image,
          &ip, NULL, &boot_image_sys, &boot_image_dev);
      rp = &rproc[boot_image_priv - boot_image_priv_table];

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
       * XXX FIXME: We should also let RS set vm calls allowed for each sys
       * service by using vm_set_priv(). We need a more uniform privilege
       * management scheme in VM for this change.
       */
      /* Force a static privilege id for system services in the boot image. */
      rp->r_priv.s_id = static_priv_id(_ENDPOINT_P(boot_image_priv->endpoint));

      /* Initialize privilege bitmaps. */
      rp->r_priv.s_flags = boot_image_priv->flags;         /* privilege flags */
      rp->r_priv.s_trap_mask = boot_image_priv->trap_mask; /* allowed traps */
      memcpy(&rp->r_priv.s_ipc_to, &boot_image_priv->ipc_to,
			sizeof(rp->r_priv.s_ipc_to));      /* allowed targets */

      /* Initialize call mask bitmap from unordered set. */
      fill_call_mask(boot_image_priv->k_calls, NR_SYS_CALLS,
          rp->r_priv.s_k_call_mask, KERNEL_CALL);

      /* Set the privilege structure. */
      if ((s = sys_privctl(ip->endpoint, SYS_PRIV_SET_SYS, &(rp->r_priv)))
          != OK) {
          panic("RS", "unable to set privilege structure", s);
      }

      /* Synch the privilege structure with the kernel. */
      if ((s = sys_getpriv(&(rp->r_priv), ip->endpoint)) != OK) {
          panic("RS", "unable to synch privilege structure", s);
      }

      /*
       * Set sys properties.
       */
      rp->r_sys_flags = boot_image_sys->flags;        /* sys flags */

      /*
       * Set dev properties.
       */
      rp->r_dev_nr = boot_image_dev->dev_nr;          /* major device number */
      rp->r_dev_style = boot_image_dev->dev_style;    /* device style */
      rp->r_period = boot_image_dev->period;          /* heartbeat period */
  }

  /* - Step 2: allow every system service in the boot image to run.
   */
  for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
      boot_image_priv = &boot_image_priv_table[i];

      /* System services only. */
      if(!isbootsrvprocn(_ENDPOINT_P(boot_image_priv->endpoint))) {
          continue;
      }

      /* Lookup the corresponding entry in the boot image table. */
      boot_image_info_lookup(boot_image_priv->endpoint, image,
          &ip, NULL, NULL, NULL);

      /* Allow the process to run. */
      if ((s = sys_privctl(ip->endpoint, SYS_PRIV_ALLOW, NULL)) != OK) {
          panic("RS", "unable to initialize privileges", s);
      }
  }

  /* - Step 3: all the system services in the boot image are now running. Use
   * the boot image table from the kernel and PM process table to complete
   * the initialization of the system process table.
   */
  if ((s = getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc)) != OK) {
      panic("RS", "unable to get copy of PM process table", s);
  }
  for (i=0; boot_image_priv_table[i].endpoint != NULL_BOOT_NR; i++) {
      boot_image_priv = &boot_image_priv_table[i];

      /* System services only. */
      if(!isbootsrvprocn(_ENDPOINT_P(boot_image_priv->endpoint))) {
          continue;
      }

      /* Lookup the corresponding entry in the boot image table. */
      boot_image_info_lookup(boot_image_priv->endpoint, image,
          &ip, NULL, NULL, NULL);
      rp = &rproc[boot_image_priv - boot_image_priv_table];

      /* Get label. */
      strcpy(rp->r_label, ip->proc_name);

      /* Get command settings. */
      rp->r_cmd[0]= '\0';
      rp->r_argv[0] = rp->r_cmd;
      rp->r_argv[1] = NULL;
      rp->r_argc = 1;
      rp->r_script[0]= '\0';

      /* Get settings from the boot image table. */
      rp->r_nice = ip->priority;
      rp->r_proc_nr_e = ip->endpoint;

      /* Get pid from PM process table. */
      rp->r_pid = NO_PID;
      for (j = 0; j < NR_PROCS; j++) {
          if (mproc[j].mp_endpoint == rp->r_proc_nr_e) {
              rp->r_pid = mproc[j].mp_pid;
              break;
          }
      }
      if(j == NR_PROCS) {
          panic("RS", "unable to get pid", NO_NUM);
      }

      /* Set some defaults. */
      rp->r_uid = 0;                           /* root */
      rp->r_check_tm = 0;                      /* not checked yet */
      getuptime(&rp->r_alive_tm);              /* currently alive */
      rp->r_stop_tm = 0;                       /* not exiting yet */
      rp->r_restarts = 0;                      /* no restarts so far */
      rp->r_set_resources = 0;                 /* no resources */

      /* Mark as in use. */
      rp->r_flags = RS_IN_USE;
      rproc_ptr[_ENDPOINT_P(rp->r_proc_nr_e)]= rp;

      /* Publish the new system service. */
      s = publish_service(rp);
      if (s != OK) {
          panic("RS", "unable to publish boot system service", s);
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

