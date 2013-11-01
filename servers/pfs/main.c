#include "fs.h"
#include <assert.h>
#include <signal.h>
#include <minix/dmap.h>
#include <minix/driver.h>
#include <minix/endpoint.h>
#include <minix/rs.h>
#include <minix/vfsif.h>
#include <sys/types.h>
#include <pwd.h>
#include "buf.h"
#include "inode.h"

static void get_work(message *m_in, int *status);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
int main(int argc, char *argv[])
{
/* This is the main routine of this service. The main loop consists of
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  int ind, transid, req_nr, ipc_status;
  message pfs_m_in;
  message pfs_m_out;
  endpoint_t src;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  while(!unmountdone || !exitsignaled) {
	/* Wait for request message. */
	get_work(&pfs_m_in, &ipc_status);

	transid = TRNS_GET_ID(pfs_m_in.m_type);
	pfs_m_in.m_type = TRNS_DEL_ID(pfs_m_in.m_type);
	if (pfs_m_in.m_type == 0) {
		assert(!IS_VFS_FS_TRANSID(transid));
		pfs_m_in.m_type = transid;
		transid = 0;
	} else
		assert(IS_VFS_FS_TRANSID(transid) || transid == 0);

	src = pfs_m_in.m_source;
	req_nr = pfs_m_in.m_type;

	if (IS_FS_RQ(req_nr)) {
		ind = req_nr - FS_BASE;
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

	if (IS_FS_RQ(req_nr) && IS_VFS_FS_TRANSID(transid)) {
		pfs_m_out.m_type = TRNS_ADD_ID(pfs_m_out.m_type, transid);
	}
	reply(src, &pfs_m_out);
  }
  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
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
static int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the pipe file server. */
  int i;
  struct passwd *pw;

  /* Initialize main loop parameters. */
  exitsignaled = 0;	/* No exit request seen yet. */
  busy = 0;		/* Server is not 'busy' (i.e., inodes in use). */

  /* Init inode table */
  for (i = 0; i < PFS_NR_INODES; ++i) {
	inode[i].i_count = 0;
  }

  init_inode_cache();
  buf_pool();

  /* Drop root privileges */
  if ((pw = getpwnam(SERVICE_LOGIN)) == NULL) {
	printf("PFS: unable to retrieve uid of SERVICE_LOGIN, "
		"still running as root");
  } else if (setuid(pw->pw_uid) != 0) {
	panic("unable to drop privileges");
  }

  return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;


  exitsignaled = 1;
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
static void get_work(message * m_in, int *status)
{
  int r, srcok = 0;
  endpoint_t src;

  do {
	/* wait for a message */
	if ((r = sef_receive_status(ANY, m_in, status)) != OK)
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
void reply(who, m_out)
endpoint_t who;
message *m_out;                       	/* report result */
{
  int r;

  if (OK != (r = ipc_send(who, m_out)))	/* send the message */
	printf("PFS: unable to send reply: %d\n", r);
}
