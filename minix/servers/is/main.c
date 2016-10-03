/* System Information Service.
 * This service handles the various debugging dumps, such as the process
 * table, so that these no longer directly touch kernel memory. Instead, the
 * system task is asked to copy some table in local memory.
 *
 * Created:
 *   Apr 29, 2004	by Jorrit N. Herder
 */

#include "inc.h"
#include <minix/endpoint.h>

/* Allocate space for the global variables. */
static message m_in;		/* the input message itself */
static message m_out;		/* the output message used for reply */
static endpoint_t who_e;	/* caller's proc number */
static int callnr;		/* system call number */

/* Declare some local functions. */
static void get_work(void);
static void reply(int whom, int result);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
int main(int argc, char **argv)
{
/* This is the main routine of this service. The main loop consists of
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  int result;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  /* Main loop - get work and do it, forever. */
  while (TRUE) {
      /* Wait for incoming message, sets 'callnr' and 'who'. */
      get_work();

      if (is_notify(callnr)) {
	      switch (_ENDPOINT_P(who_e)) {
		      case TTY_PROC_NR:
			      result = do_fkey_pressed(&m_in);
			      break;
		      default:
			      /* FIXME: error message. */
			      result = EDONTREPLY;
			      break;
	      }
      }
      else {
          printf("IS: warning, got illegal request %d from %d\n",
          	callnr, m_in.m_source);
          result = EDONTREPLY;
      }

      /* Finally send reply message, unless disabled. */
      if (result != EDONTREPLY) {
	  reply(who_e, result);
      }
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void
sef_local_startup(void)
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the information server. */

  /* Set key mappings. */
  map_unmap_fkeys(TRUE /*map*/);

  return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* Shutting down. Unset key mappings, and quit. */
  map_unmap_fkeys(FALSE /*map*/);

  exit(0);
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
static void
get_work(void)
{
    int status = 0;
    status = sef_receive(ANY, &m_in);   /* this blocks until message arrives */
    if (OK != status)
        panic("sef_receive failed!: %d", status);
    who_e = m_in.m_source;        /* message arrived! set sender */
    callnr = m_in.m_type;       /* set function call number */
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void
reply(
	int who,                           	/* destination */
	int result                           	/* report result to replyee */
)
{
    int send_status;
    m_out.m_type = result;  		/* build reply message */
    send_status = ipc_send(who, &m_out);    /* send the message */
    if (OK != send_status)
        panic("unable to send reply!: %d", send_status);
}


