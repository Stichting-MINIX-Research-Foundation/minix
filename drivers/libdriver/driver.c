/* This file contains device independent device driver interface.
 *
 * Changes:
 *   Jul 25, 2005   added SYS_SIG type for signals  (Jorrit N. Herder)
 *   Sep 15, 2004   added SYN_ALARM type for timeouts  (Jorrit N. Herder)
 *   Jul 23, 2004   removed kernel dependencies  (Jorrit N. Herder)
 *   Apr 02, 1992   constructed from AT wini and floppy driver  (Kees J. Bot)
 *
 *
 * The drivers support the following operations (using message format m2):
 *
 *    m_type      DEVICE    IO_ENDPT    COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_GATHER | device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_SCATTER| device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_IOCTL | device  | proc nr |func code|         | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  CANCEL    | device  | proc nr | r/w     |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  HARD_STOP |         |         |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_*_S   | variants using safecopies of above              |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   driver_task:	called by the device dependent task entry
 */


#include "../drivers.h"
#include <sys/ioc_disk.h>
#include <minix/mq.h>
#include "driver.h"

/* Claim space for variables. */
#if 0
PRIVATE u8_t buffer[(unsigned) 2 * DMA_BUF_SIZE];
#endif
u8_t *tmp_buf;			/* the DMA buffer eventually */
phys_bytes tmp_phys;		/* phys address of DMA buffer */

FORWARD _PROTOTYPE( int do_rdwt, (struct driver *dr, message *mp, int safe) );
FORWARD _PROTOTYPE( int do_vrdwt, (struct driver *dr, message *mp, int safe) );

int device_caller;
PRIVATE mq_t *queue_head = NULL;

/*===========================================================================*
 *				driver_task				     *
 *===========================================================================*/
PUBLIC void driver_task(dp)
struct driver *dp;	/* Device dependent entry points. */
{
/* Main program of any device driver task. */

  int r, proc_nr;
  message mess;

  /* Init MQ library. */
  mq_init();

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (TRUE) {
	/* Any queued messages? Oldest are at the head. */
	if(queue_head) {
		mq_t *mq;
		mq = queue_head;
		memcpy(&mess, &mq->mq_mess, sizeof(mess));
		queue_head = queue_head->mq_next;
		mq_free(mq);
	} else {
		int s;
		/* Wait for a request to read or write a disk block. */
		if ((s=receive(ANY, &mess)) != OK)
        		panic((*dp->dr_name)(),"receive() failed", s);
	}

	device_caller = mess.m_source;
	proc_nr = mess.IO_ENDPT;

	/* Now carry out the work. */
	switch(mess.m_type) {
	case DEV_OPEN:		r = (*dp->dr_open)(dp, &mess);	break;	
	case DEV_CLOSE:		r = (*dp->dr_close)(dp, &mess);	break;
#ifdef DEV_IOCTL
	case DEV_IOCTL:		r = (*dp->dr_ioctl)(dp, &mess, 0); break;
#endif
	case DEV_IOCTL_S:	r = (*dp->dr_ioctl)(dp, &mess, 1); break;
	case CANCEL:		r = (*dp->dr_cancel)(dp, &mess);break;
	case DEV_SELECT:	r = (*dp->dr_select)(dp, &mess);break;
#ifdef DEV_READ
	case DEV_READ:	
	case DEV_WRITE:	  	r = do_rdwt(dp, &mess, 0); break;
#endif
	case DEV_READ_S:	
	case DEV_WRITE_S:  	r = do_rdwt(dp, &mess, 1); break;
#ifdef DEV_GATHER
	case DEV_GATHER: 
	case DEV_SCATTER: 	r = do_vrdwt(dp, &mess, 0); break;
#endif
	case DEV_GATHER_S: 
	case DEV_SCATTER_S: 	r = do_vrdwt(dp, &mess, 1); break;

	case HARD_INT:		/* leftover interrupt or expired timer. */
				if(dp->dr_hw_int) {
					(*dp->dr_hw_int)(dp, &mess);
				}
				continue;
	case PROC_EVENT:
	case SYS_SIG:		(*dp->dr_signal)(dp, &mess);
				continue;	/* don't reply */
	case SYN_ALARM:		(*dp->dr_alarm)(dp, &mess);	
				continue;	/* don't reply */
	case DEV_PING:		notify(mess.m_source);
				continue;
	default:		
		if(dp->dr_other)
			r = (*dp->dr_other)(dp, &mess, 0);
		else	
			r = EINVAL;
		break;
	}

	/* Clean up leftover state. */
	(*dp->dr_cleanup)();

	/* Finally, prepare and send the reply message. */
	if (r != EDONTREPLY) {
		mess.m_type = TASK_REPLY;
		mess.REP_ENDPT = proc_nr;
		/* Status is # of bytes transferred or error code. */
		mess.REP_STATUS = r;	
		r= sendnb(device_caller, &mess);
		if (r != OK)
		{
			printf("driver_task: unable to sendnb to %d: %d\n",
				device_caller, r);
		}
	}
  }
}


/*===========================================================================*
 *				init_buffer				     *
 *===========================================================================*/
PUBLIC void init_buffer(void)
{
/* Select a buffer that can safely be used for DMA transfers.  It may also
 * be used to read partition tables and such.  Its absolute address is
 * 'tmp_phys', the normal address is 'tmp_buf'.
 */

  unsigned left;

  if(!(tmp_buf = alloc_contig(2*DMA_BUF_SIZE, AC_ALIGN4K, &tmp_phys)))
	panic(__FILE__, "can't allocate tmp_buf", DMA_BUF_SIZE);
}

/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
PRIVATE int do_rdwt(dp, mp, safe)
struct driver *dp;		/* device dependent entry points */
message *mp;			/* pointer to read or write message */
int safe;			/* use safecopies? */
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  int r, opcode;
  phys_bytes phys_addr;
  u64_t position;

  /* Disk address?  Address and length of the user buffer? */
  if (mp->COUNT < 0) return(EINVAL);

  /* Check the user buffer (not relevant for safe copies). */
  if(!safe) {
	  printf("libdriver_asyn: do_rdwt: no support for non-safe command.\n");
	  return EINVAL;
  }

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Create a one element scatter/gather vector for the buffer. */
  if(
#ifdef DEV_READ
  mp->m_type == DEV_READ || 
#endif
  mp->m_type == DEV_READ_S) opcode = DEV_GATHER_S;
  else	opcode =  DEV_SCATTER_S;

  iovec1.iov_addr = (vir_bytes) mp->ADDRESS;
  iovec1.iov_size = mp->COUNT;

  /* Transfer bytes from/to the device. */
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->IO_ENDPT, opcode, position, &iovec1, 1, safe);

  /* Return the number of bytes transferred or an error code. */
  return(r == OK ? (mp->COUNT - iovec1.iov_size) : r);
}

/*==========================================================================*
 *				do_vrdwt				    *
 *==========================================================================*/
PRIVATE int do_vrdwt(dp, mp, safe)
struct driver *dp;	/* device dependent entry points */
message *mp;		/* pointer to read or write message */
int safe;		/* use safecopies? */
{
/* Carry out an device read or write to/from a vector of user addresses.
 * The "user addresses" are assumed to be safe, i.e. FS transferring to/from
 * its own buffers, so they are not checked.
 */
  static iovec_t iovec[NR_IOREQS];
  iovec_t *iov;
  phys_bytes iovec_size;
  unsigned nr_req;
  int r, j, opcode;
  u64_t position;

  nr_req = mp->COUNT;	/* Length of I/O vector */

  {
    /* Copy the vector from the caller to kernel space. */
    if (nr_req > NR_IOREQS) nr_req = NR_IOREQS;
    iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));

    if(safe) {
	    if (OK != sys_safecopyfrom(mp->m_source, (vir_bytes) mp->IO_GRANT, 
    			0, (vir_bytes) iovec, iovec_size, D)) {
        	panic((*dp->dr_name)(),"bad (safe) I/O vector by", mp->m_source);
	    }
    } else {
	    if (OK != sys_datacopy(mp->m_source, (vir_bytes) mp->ADDRESS, 
    			SELF, (vir_bytes) iovec, iovec_size)) {
        	panic((*dp->dr_name)(),"bad I/O vector by", mp->m_source);
	    }
    }

    iov = iovec;
  }

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Transfer bytes from/to the device. */
  opcode = mp->m_type;
  position= make64(mp->POSITION, mp->HIGHPOS);
  r = (*dp->dr_transfer)(mp->IO_ENDPT, opcode, position, iov,
	nr_req, safe);

  /* Copy the I/O vector back to the caller. */
  if(safe) {
    if (OK != sys_safecopyto(mp->m_source, (vir_bytes) mp->IO_GRANT, 
    		0, (vir_bytes) iovec, iovec_size, D)) {
        panic((*dp->dr_name)(),"couldn't return I/O vector", mp->m_source);
    }
  } else {
    sys_datacopy(SELF, (vir_bytes) iovec, 
    	mp->m_source, (vir_bytes) mp->ADDRESS, iovec_size);
  }

  return(r);
}

/*===========================================================================*
 *				no_name					     *
 *===========================================================================*/
PUBLIC char *no_name()
{
/* Use this default name if there is no specific name for the device. This was
 * originally done by fetching the name from the task table for this process: 
 * "return(tasktab[proc_number(proc_ptr) + NR_TASKS].name);", but currently a
 * real "noname" is returned. Perhaps, some system information service can be
 * queried for a name at a later time.
 */
  static char name[] = "noname";
  return name;
}

/*============================================================================*
 *				do_nop					      *
 *============================================================================*/
PUBLIC int do_nop(dp, mp)
struct driver *dp;
message *mp;
{
/* Nothing there, or nothing to do. */

  switch (mp->m_type) {
  case DEV_OPEN:	return(ENODEV);
  case DEV_CLOSE:	return(OK);
  case DEV_IOCTL_S:	
#ifdef DEV_IOCTL
  case DEV_IOCTL:	return(ENOTTY);
#endif
  default:		printf("nop: ignoring code %d\n", mp->m_type); return(EIO);
  }
}

/*============================================================================*
 *				nop_ioctl				      *
 *============================================================================*/
PUBLIC int nop_ioctl(dp, mp, safe)
struct driver *dp;
message *mp;
int safe;
{
  return(ENOTTY);
}

/*============================================================================*
 *				nop_signal			  	      *
 *============================================================================*/
PUBLIC void nop_signal(dp, mp)
struct driver *dp;
message *mp;
{
/* Default action for signal is to ignore. */
}

/*============================================================================*
 *				nop_alarm				      *
 *============================================================================*/
PUBLIC void nop_alarm(dp, mp)
struct driver *dp;
message *mp;
{
/* Ignore the leftover alarm. */
}

/*===========================================================================*
 *				nop_prepare				     *
 *===========================================================================*/
PUBLIC struct device *nop_prepare(device)
{
/* Nothing to prepare for. */
  return(NIL_DEV);
}

/*===========================================================================*
 *				nop_cleanup				     *
 *===========================================================================*/
PUBLIC void nop_cleanup()
{
/* Nothing to clean up. */
}

/*===========================================================================*
 *				nop_cancel				     *
 *===========================================================================*/
PUBLIC int nop_cancel(struct driver *dr, message *m)
{
/* Nothing to do for cancel. */
   return(OK);
}

/*===========================================================================*
 *				nop_select				     *
 *===========================================================================*/
PUBLIC int nop_select(struct driver *dr, message *m)
{
/* Nothing to do for select. */
   return(OK);
}

/*============================================================================*
 *				do_diocntl				      *
 *============================================================================*/
PUBLIC int do_diocntl(dp, mp, safe)
struct driver *dp;
message *mp;			/* pointer to ioctl request */
int safe;			/* addresses or grants? */
{
/* Carry out a partition setting/getting request. */
  struct device *dv;
  struct partition entry;
  int s;

  if (mp->REQUEST != DIOCSETP && mp->REQUEST != DIOCGETP) {
  	if(dp->dr_other) {
  		return dp->dr_other(dp, mp, safe);
  	} else return(ENOTTY);
  }

  /* Decode the message parameters. */
  if ((dv = (*dp->dr_prepare)(mp->DEVICE)) == NIL_DEV) return(ENXIO);

  if (mp->REQUEST == DIOCSETP) {
	/* Copy just this one partition table entry. */
	if(safe) {
	  s=sys_safecopyfrom(mp->IO_ENDPT, (vir_bytes) mp->IO_GRANT, 
    			0, (vir_bytes) &entry, sizeof(entry), D);
	} else{
	  s=sys_datacopy(mp->IO_ENDPT, (vir_bytes) mp->ADDRESS,
		SELF, (vir_bytes) &entry, sizeof(entry));
	}
	if(s != OK)
	    return s;
	dv->dv_base = entry.base;
	dv->dv_size = entry.size;
  } else {
	/* Return a partition table entry and the geometry of the drive. */
	entry.base = dv->dv_base;
	entry.size = dv->dv_size;
	(*dp->dr_geometry)(&entry);
	if(safe) {
	  s=sys_safecopyto(mp->IO_ENDPT, (vir_bytes) mp->IO_GRANT, 
    			0, (vir_bytes) &entry, sizeof(entry), D);
	} else {
	  s=sys_datacopy(SELF, (vir_bytes) &entry,
		mp->IO_ENDPT, (vir_bytes) mp->ADDRESS, sizeof(entry));
	}
        if (OK != s) 
	    return s;
  }
  return(OK);
}

/*===========================================================================*
 *				mq_queue				     *
 *===========================================================================*/
PUBLIC int mq_queue(message *m)
{
	mq_t *mq, *mi;

	if(!(mq = mq_get()))
        	panic("libdriver","mq_queue: mq_get failed", NO_NUM);
	memcpy(&mq->mq_mess, m, sizeof(mq->mq_mess));
	mq->mq_next = NULL;
	if(!queue_head) {
		queue_head = mq;
	} else {
		for(mi = queue_head; mi->mq_next; mi = mi->mq_next)
			;
		mi->mq_next = mq;
	}

	return OK;
}

