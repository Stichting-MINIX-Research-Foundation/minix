/* This file contains the main program of the File System.  It consists of
 * a loop that gets messages requesting work, carries out the work, and sends
 * replies.
 *
 * The entry points into this file are:
 *   main:	main program of the File System
 *   reply:	send a reply to a process after the requested work is done
 *
 */

struct super_block;		/* proto.h needs to know this */

#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioc_memory.h>
#include <sys/svrctl.h>
#include <sys/select.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

FORWARD _PROTOTYPE( void fs_init, (void)				);
FORWARD _PROTOTYPE( void get_work, (void)				);
FORWARD _PROTOTYPE( void init_root, (void)				);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main()
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  int error;

  fs_init();


  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	get_work();		/* sets who and call_nr */
	fp = &fproc[who_p];	/* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */

 	/* Check for special control messages first. */
	if (call_nr == PROC_EVENT) {
		/* Assume FS got signal. Synchronize, but don't exit. */
		do_sync();
        } else if (call_nr == SYN_ALARM) {
        	/* Alarm timer expired. Used only for select(). Check it. */
        	fs_expire_timers(m_in.NOTIFY_TIMESTAMP);
        } else if ((call_nr & NOTIFY_MESSAGE)) {
        	/* Device notifies us of an event. */
        	dev_status(&m_in);
        } else {
		/* Call the internal function that does the work. */
		if (call_nr < 0 || call_nr >= NCALLS) { 
			error = ENOSYS;
		/* Not supposed to happen. */
			printf("FS, warning illegal %d system call by %d\n", call_nr, who_e);
		} else if (fp->fp_pid == PID_FREE) {
			error = ENOSYS;
			printf("FS, bad process, who = %d, call_nr = %d, endpt1 = %d\n",
				 who_e, call_nr, m_in.endpt1);
		} else {
			error = (*call_vec[call_nr])();
		}

		/* Copy the results back to the user and send reply. */
		if (error != SUSPEND) { reply(who_e, error); }
		if (rdahed_inode != NIL_INODE) {
			read_ahead(); /* do block read ahead */
		}
	}
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *				get_work				     *
 *===========================================================================*/
PRIVATE void get_work()
{  
  /* Normally wait for new input.  However, if 'reviving' is
   * nonzero, a suspended process must be awakened.
   */
  register struct fproc *rp;

  if (reviving != 0) {
	/* Revive a suspended process. */
	for (rp = &fproc[0]; rp < &fproc[NR_PROCS]; rp++) 
		if (rp->fp_pid != PID_FREE && rp->fp_revived == REVIVING) {
			who_p = (int)(rp - fproc);
			who_e = rp->fp_endpoint;
			call_nr = rp->fp_fd & BYTE;
			m_in.fd = (rp->fp_fd >>8) & BYTE;
			m_in.buffer = rp->fp_buffer;
			m_in.nbytes = rp->fp_nbytes;
			rp->fp_suspended = NOT_SUSPENDED; /*no longer hanging*/
			rp->fp_revived = NOT_REVIVING;
			reviving--;
			return;
		}
	panic(__FILE__,"get_work couldn't revive anyone", NO_NUM);
  }

  for(;;) {
    /* Normal case.  No one to revive. */
    if (receive(ANY, &m_in) != OK)
	panic(__FILE__,"fs receive error", NO_NUM);
    who_e = m_in.m_source;
    who_p = _ENDPOINT_P(who_e);
    if(who_p < -NR_TASKS || who_p >= NR_PROCS)
     	panic(__FILE__,"receive process out of range", who_p);
    if(who_p >= 0 && fproc[who_p].fp_endpoint == NONE) {
    	printf("FS: ignoring request from %d, endpointless slot %d (%d)\n",
		m_in.m_source, who_p, m_in.m_type);
	continue;
    }
    if(who_p >= 0 && fproc[who_p].fp_endpoint != who_e) {
    	printf("FS: receive endpoint inconsistent (%d, %d, %d).\n",
		who_e, fproc[who_p].fp_endpoint, who_e);
	panic(__FILE__, "FS: inconsistent endpoint ", NO_NUM);
	continue;
    }
    call_nr = m_in.m_type;
    return;
  }
}

/*===========================================================================*
 *				buf_pool				     *
 *===========================================================================*/
PRIVATE void buf_pool(void)
{
/* Initialize the buffer pool. */

  register struct buf *bp;

  bufs_in_use = 0;
  front = &buf[0];
  rear = &buf[NR_BUFS - 1];

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) {
	bp->b_blocknr = NO_BLOCK;
	bp->b_dev = NO_DEV;
	bp->b_next = bp + 1;
	bp->b_prev = bp - 1;
  }
  buf[0].b_prev = NIL_BUF;
  buf[NR_BUFS - 1].b_next = NIL_BUF;

  for (bp = &buf[0]; bp < &buf[NR_BUFS]; bp++) bp->b_hash = bp->b_next;
  buf_hash[0] = front;

}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(whom, result)
int whom;			/* process to reply to */
int result;			/* result of the call (usually OK or error #) */
{
/* Send a reply to a user process.  If the send fails, just ignore it. */
  int s;
  m_out.reply_type = result;
  s = send(whom, &m_out);
  if (s != OK) printf("FS: couldn't send reply %d to %d: %d\n",
	result, whom, s);
}

/*===========================================================================*
 *				fs_init					     *
 *===========================================================================*/
PRIVATE void fs_init()
{
/* Initialize global variables, tables, etc. */
  register struct inode *rip;
  register struct fproc *rfp;
  message mess;
  int s;

  /* Initialize the process table with help of the process manager messages. 
   * Expect one message for each system process with its slot number and pid. 
   * When no more processes follow, the magic process number NONE is sent. 
   * Then, stop and synchronize with the PM.
   */
  do {
  	if (OK != (s=receive(PM_PROC_NR, &mess)))
  		panic(__FILE__,"FS couldn't receive from PM", s);
  	if (NONE == mess.PR_ENDPT) break; 

	rfp = &fproc[mess.PR_SLOT];
	rfp->fp_pid = mess.PR_PID;
	rfp->fp_endpoint = mess.PR_ENDPT;
	rfp->fp_realuid = (uid_t) SYS_UID;
	rfp->fp_effuid = (uid_t) SYS_UID;
	rfp->fp_realgid = (gid_t) SYS_GID;
	rfp->fp_effgid = (gid_t) SYS_GID;
	rfp->fp_umask = ~0;
   
  } while (TRUE);			/* continue until process NONE */
  mess.m_type = OK;			/* tell PM that we succeeded */
  s=send(PM_PROC_NR, &mess);		/* send synchronization message */

  /* All process table entries have been set. Continue with FS initialization.
   * Certain relations must hold for the file system to work at all. Some 
   * extra block_size requirements are checked at super-block-read-in time.
   */
  if (OPEN_MAX > 127) panic(__FILE__,"OPEN_MAX > 127", NO_NUM);
  if (NR_BUFS < 6) panic(__FILE__,"NR_BUFS < 6", NO_NUM);
  if (V1_INODE_SIZE != 32) panic(__FILE__,"V1 inode size != 32", NO_NUM);
  if (V2_INODE_SIZE != 64) panic(__FILE__,"V2 inode size != 64", NO_NUM);
  if (OPEN_MAX > 8 * sizeof(long))
  	 panic(__FILE__,"Too few bits in fp_cloexec", NO_NUM);

  /* The following initializations are needed to let dev_opcl succeed .*/
  fp = (struct fproc *) NULL;
  who_e = who_p = FS_PROC_NR;

  buf_pool();			/* initialize buffer pool */
  build_dmap();			/* build device table and map boot driver */
  init_root();			/* init root device and load super block */
  init_select();		/* init select() structures */

  /* The root device can now be accessed; set process directories. */
  for (rfp=&fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	FD_ZERO(&(rfp->fp_filp_inuse));
  	if (rfp->fp_pid != PID_FREE) {
		rip = get_inode(root_dev, ROOT_INODE);
		dup_inode(rip);
		rfp->fp_rootdir = rip;
		rfp->fp_workdir = rip;
  	} else  rfp->fp_endpoint = NONE;
  }
}

/*===========================================================================*
 *				init_root				     *
 *===========================================================================*/
PRIVATE void init_root()
{
  int bad;
  register struct super_block *sp;
  register struct inode *rip = NIL_INODE;
  int s;

  /* Open the root device. */
  root_dev = DEV_IMGRD;
  if ((s=dev_open(root_dev, FS_PROC_NR, R_BIT|W_BIT)) != OK)
	panic(__FILE__,"Cannot open root device", s);

#if ENABLE_CACHE2
  /* The RAM disk is a second level block cache while not otherwise used. */
  init_cache2(ram_size);
#endif

  /* Initialize the super_block table. */
  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
  	sp->s_dev = NO_DEV;

  /* Read in super_block for the root file system. */
  sp = &super_block[0];
  sp->s_dev = root_dev;

  /* Check super_block for consistency. */
  bad = (read_super(sp) != OK);
  if (!bad) {
	rip = get_inode(root_dev, ROOT_INODE);	/* inode for root dir */
	if ( (rip->i_mode & I_TYPE) != I_DIRECTORY || rip->i_nlinks < 3) bad++;
  }
  if (bad) panic(__FILE__,"Invalid root file system", NO_NUM);

  sp->s_imount = rip;
  dup_inode(rip);
  sp->s_isup = rip;
  sp->s_rd_only = 0;
  return;
}
