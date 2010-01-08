#include "fs.h"
#include <assert.h>
#include <minix/dmap.h>
#include <minix/endpoint.h>
#include <minix/vfsif.h>
#include "buf.h"
#include "inode.h"
#include "drivers.h"

FORWARD _PROTOTYPE(void get_work, (message *m_in)			);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char *argv[])
{
/* This is the main routine of this service. The main loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  int error, ind;
  message m;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  while(!exitsignaled || busy) {
	endpoint_t src;

	/* Wait for request message. */
	get_work(&fs_m_in);
	
	src = fs_m_in.m_source;
	error = OK;
	caller_uid = -1;	/* To trap errors */
	caller_gid = -1;

	if (src == PM_PROC_NR) continue; /* Exit signal */
	assert(src == VFS_PROC_NR); /* Otherwise this must be VFS talking */
	req_nr = fs_m_in.m_type;
	if (req_nr < VFS_BASE) {
		fs_m_in.m_type += VFS_BASE;
		req_nr = fs_m_in.m_type;
	}
	ind = req_nr - VFS_BASE;

	if (ind < 0 || ind >= NREQS) {
		printf("pfs: bad request %d\n", req_nr); 
		printf("ind = %d\n", ind);
		error = EINVAL; 
	} else {
		error = (*fs_call_vec[ind])();
	}

	fs_m_out.m_type = error; 
	reply(src, &fs_m_out);
      
  }
  return(OK);
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
/* Initialize the pipe file server. */
  int i;

  /* Initialize main loop parameters. */
  exitsignaled = 0;	/* No exit request seen yet. */
  busy = 0;		/* Server is not 'busy' (i.e., inodes in use). */

  /* Init inode table */
  for (i = 0; i < NR_INODES; ++i) {
	inode[i].i_count = 0;
	cch[i] = 0;
  }
	
  init_inode_cache();

  /* Init driver mapping */
  for (i = 0; i < NR_DEVICES; ++i) 
	driver_endpoints[i].driver_e = NONE;
	
  SELF_E = getprocnr();
  buf_pool();

  return(OK);
}


/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work(m_in)
message *m_in;				/* pointer to message */
{
  int r, srcok = 0;
  endpoint_t src;

  do {
	if ((r = sef_receive(ANY, m_in)) != OK) 	/* wait for message */
		panic("PFS","sef_receive failed", r);
	src = fs_m_in.m_source;

	if (src != VFS_PROC_NR) {
		if(src == PM_PROC_NR) {
			if(is_notify(fs_m_in.m_type)) {
				exitsignaled = 1; /* Normal exit request. */
				srcok = 1;	
			} else
				printf("PFS: unexpected message from PM\n");
		} else
			printf("PFS: unexpected source %d\n", src);
	} else if(src == VFS_PROC_NR) {
		srcok = 1;		/* Normal FS request. */
	} else
		printf("PFS: unexpected source %d\n", src);
  } while(!srcok);

   assert( src == VFS_PROC_NR || 
	  (src == PM_PROC_NR && is_notify(fs_m_in.m_type))
	 );
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(who, m_out)
int who;	
message *m_out;                       	/* report result */
{
  if (OK != send(who, m_out))    /* send the message */
	printf("PFS(%d) was unable to send reply\n", SELF_E);
}

