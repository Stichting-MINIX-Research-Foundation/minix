

/* This file contains the main directory for the server. It waits for a 
 * request and then send a response. */

#include "inc.h"
#include <minix/vfsif.h>
#include "const.h"
#include "glo.h"

/* Declare some local functions. */
FORWARD _PROTOTYPE(void get_work, (message *m_in)			);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(void) {
  int who_e, ind, error;
  message m;

  /* SEF local startup. */
  sef_local_startup();

  for (;;) {

	/* Wait for request message. */
	get_work(&fs_m_in);
	error = OK;

	caller_uid = -1;	/* To trap errors */
	caller_gid = -1;

	who_e = fs_m_in.m_source;	/* source of the request */

	if (who_e != FS_PROC_NR) { /* If the message is not for us just 
				    * continue */
		continue;
	}

	req_nr = fs_m_in.m_type;

	if (req_nr < VFS_BASE) {
		fs_m_in.m_type += VFS_BASE;
		req_nr = fs_m_in.m_type;
	}

	ind = req_nr-VFS_BASE;

	if (ind < 0 || ind >= NREQS) {
		error = EINVAL; 
	} else
		error = (*fs_call_vec[ind])(); /* Process the request calling
						* the appropriate function. */

	fs_m_out.m_type = error; 
	reply(who_e, &fs_m_out); 	/* returns the response to VFS */
  }
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_restart_fail);

  /* No live update support for now. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the iso9660fs server. */
   int i, r;

   /* Init driver mapping */
   for (i = 0; i < NR_DEVICES; ++i) 
       driver_endpoints[i].driver_e = NONE;
   /* SELF_E will contain the id of this process */
   SELF_E = getprocnr();
/*    hash_init(); */			/* Init the table with the ids */
   setenv("TZ","",1);		/* Used to calculate the time */

   fs_m_in.m_type = FS_READY;

   if ((r = send(FS_PROC_NR, &fs_m_in)) != OK) {
       panic("ISOFS", "Error sending login to VFS", r);
   }

   return(OK);
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
PRIVATE void get_work(m_in)
message *m_in;				/* pointer to message */
{
  int s;					/* receive status */
  if (OK != (s = sef_receive(ANY, m_in))) 	/* wait for message */
    panic("ISOFS","sef_receive failed", s);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(who, m_out)
int who;	
message *m_out;                       	/* report result */
{
  if (OK != send(who, m_out))    /* send the message */
    printf("ISOFS(%d) was unable to send reply\n", SELF_E);
}
