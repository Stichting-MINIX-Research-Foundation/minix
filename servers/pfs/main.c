#include "fs.h"
#include <assert.h>
#include <signal.h>
#include <minix/dmap.h>
#include <minix/driver.h>
#include <minix/endpoint.h>
#include <minix/vfsif.h>
#include "buf.h"
#include "inode.h"
#include "uds.h"

FORWARD _PROTOTYPE(void get_work, (message *m_in)			);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( void sef_cb_signal_handler, (int signo) );

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char *argv[])
{
/* This is the main routine of this service. The main loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  int ind;
  message pfs_m_in;
  message pfs_m_out;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  while(!exitsignaled || busy) {
	endpoint_t src;

	/* Wait for request message. */
	get_work(&pfs_m_in);

	src = pfs_m_in.m_source;
	caller_uid = INVAL_UID;	/* To trap errors */
	caller_gid = INVAL_GID;
	req_nr = pfs_m_in.m_type;

	if (IS_DEV_RQ(req_nr)) {
		ind = req_nr - DEV_RQ_BASE;
		if (ind < 0 || ind >= DEV_CALL_VEC_SIZE) {
			printf("pfs: bad DEV request %d\n", req_nr);
			pfs_m_out.m_type = EINVAL;
		} else {
			(*dev_call_vec[ind])(&pfs_m_in, &pfs_m_out);
		}
	} else if (IS_VFS_RQ(req_nr)) {
		ind = req_nr - VFS_BASE;
		if (ind < 0 || ind >= FS_CALL_VEC_SIZE) {
			printf("pfs: bad FS request %d\n", req_nr);
			pfs_m_out.m_type = EINVAL;
		} else {
			pfs_m_out.m_type =
				(*fs_call_vec[ind])(&pfs_m_in, &pfs_m_out);
		}
	} else {
		printf("pfs: bad request %d\n", req_nr);
		pfs_m_out.m_type = EINVAL;
	}

	reply(src, &pfs_m_out);
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
  sef_setcb_init_restart(sef_cb_init_fail);

  /* No live update support for now. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

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
  }
	
  init_inode_cache();
  uds_init();

  SELF_E = getprocnr();
  buf_pool();

  driver_announce();

  return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  exitsignaled = 1;
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work(m_in)
message *m_in;				/* pointer to message */
{
  int r, srcok = 0, status;
  endpoint_t src;

  do {
 	/* wait for a message */
	if ((r = sef_receive_status(ANY, m_in, &status)) != OK)
		panic("sef_receive_status failed: %d", r);
	src = m_in->m_source;

	if(src == VFS_PROC_NR) {
		srcok = 1;		/* Normal FS request. */
	} else
		printf("PFS: unexpected source %d\n", src);
  } while(!srcok);
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(who, m_out)
endpoint_t who;	
message *m_out;                       	/* report result */
{
  if (OK != send(who, m_out))    /* send the message */
	printf("PFS(%d) was unable to send reply\n", SELF_E);
}
