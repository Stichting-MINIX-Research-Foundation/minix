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
#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/keymap.h>
#include <minix/const.h>
#include "buf.h"
#include "file.h"
#include "fproc.h"
#include "inode.h"
#include "param.h"
#include "super.h"

FORWARD _PROTOTYPE( void fs_init, (void)				);
FORWARD _PROTOTYPE( int igetenv, (char *var, int optional)		);
FORWARD _PROTOTYPE( void get_work, (void)				);
FORWARD _PROTOTYPE( void load_ram, (void)				);
FORWARD _PROTOTYPE( void load_super, (Dev_t super_dev)			);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main()
{
/* This is the main program of the file system.  The main loop consists of
 * three major activities: getting new work, processing the work, and sending
 * the reply.  This loop never terminates as long as the file system runs.
 */
  sigset_t sigset;
  int error;

  fs_init();

  /* This is the main loop that gets work, processes it, and sends replies. */
  while (TRUE) {
	get_work();		/* sets who and call_nr */

	fp = &fproc[who];	/* pointer to proc table struct */
	super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */

 	/* Check for special control messages first. */
        if (call_nr == SYS_SIG) { 
		sigset = m_in.NOTIFY_ARG;
		if (sigismember(&sigset, SIGKSTOP)) {
        		do_sync();
        		sys_exit(0);  		/* never returns */
		}
        } else if (call_nr == SYN_ALARM) {
        	/* Not a user request; system has expired one of our timers,
        	 * currently only in use for select(). Check it.
        	 */
        	fs_expire_timers(m_in.NOTIFY_TIMESTAMP);
        } else if ((call_nr & NOTIFY_MESSAGE)) {
        	/* Device notifies us of an event. */
        	dev_status(&m_in);
        } else {
		/* Call the internal function that does the work. */
		if (call_nr < 0 || call_nr >= NCALLS) { 
			error = ENOSYS;
			printf("FS, warning illegal %d system call by %d\n", call_nr, who);
		} else if (fp->fp_pid == PID_FREE) {
			error = ENOSYS;
			printf("FS, bad process, who = %d, call_nr = %d, slot1 = %d\n",
				 who, call_nr, m_in.slot1);
		} else {
			error = (*call_vec[call_nr])();
		}

		/* Copy the results back to the user and send reply. */
		if (error != SUSPEND) { reply(who, error); }
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
		if (rp->fp_revived == REVIVING) {
			who = (int)(rp - fproc);
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

  /* Normal case.  No one to revive. */
  if (receive(ANY, &m_in) != OK) panic(__FILE__,"fs receive error", NO_NUM);
  who = m_in.m_source;
  call_nr = m_in.m_type;
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
/* Send a reply to a user process. It may fail (if the process has just
 * been killed by a signal), so don't check the return code.  If the send
 * fails, just ignore it.
 */
  int s;
  m_out.reply_type = result;
  s = send(whom, &m_out);
  if (s != OK) printf("FS: couldn't send reply %d: %d\n", result, s);
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
  	if (NONE == mess.PR_PROC_NR) break; 

	rfp = &fproc[mess.PR_PROC_NR];
	rfp->fp_pid = mess.PR_PID;
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
  who = FS_PROC_NR;

  buf_pool();			/* initialize buffer pool */
  build_dmap();			/* build device table and map boot driver */
  load_ram();			/* init RAM disk, load if it is root */
  load_super(root_dev);		/* load super block for root device */
  init_select();		/* init select() structures */

  /* The root device can now be accessed; set process directories. */
  for (rfp=&fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
  	if (rfp->fp_pid != PID_FREE) {
		rip = get_inode(root_dev, ROOT_INODE);
		dup_inode(rip);
		rfp->fp_rootdir = rip;
		rfp->fp_workdir = rip;
  	}
  }
}

/*===========================================================================*
 *				igetenv					     *
 *===========================================================================*/
PRIVATE int igetenv(key, optional)
char *key;
int optional;
{
/* Ask kernel for an integer valued boot environment variable. */
  char value[64];
  int i;

  if ((i = env_get_param(key, value, sizeof(value))) != OK) {
      if (!optional)
      	printf("FS: Warning, couldn't get monitor param: %d\n", i);
      return 0;
  }
  return(atoi(value));
}

/*===========================================================================*
 *				load_ram				     *
 *===========================================================================*/
PRIVATE void load_ram(void)
{
/* Allocate a RAM disk with size given in the boot parameters. If a RAM disk 
 * image is given, the copy the entire image device block-by-block to a RAM 
 * disk with the same size as the image.
 * If the root device is not set, the RAM disk will be used as root instead. 
 */
  register struct buf *bp, *bp1;
  u32_t lcount, ram_size_kb;
  zone_t zones;
  struct super_block *sp, *dsp;
  block_t b;
  Dev_t image_dev;
  static char sbbuf[MIN_BLOCK_SIZE];
  int block_size_image, block_size_ram, ramfs_block_size;
  int s;

  /* Get some boot environment variables. */
  root_dev = igetenv("rootdev", 0);
  image_dev = igetenv("ramimagedev", 0);
  ram_size_kb = igetenv("ramsize", 0);

  /* Open the root device. */
  if (dev_open(root_dev, FS_PROC_NR, R_BIT|W_BIT) != OK)
	panic(__FILE__,"Cannot open root device",NO_NUM);

  /* If we must initialize a ram disk, get details from the image device. */
  if (root_dev == DEV_RAM) {
  	u32_t fsmax, probedev;

  	/* If we are running from CD, see if we can find it. */
  	if (igetenv("cdproberoot", 1) && (probedev=cdprobe()) != NO_DEV) {
  		char devnum[10];
  		struct sysgetenv env;

  		/* If so, this is our new RAM image device. */
  		image_dev = probedev;

  		/* Tell PM about it, so userland can find out about it
  		 * with sysenv interface.
  		 */
  		env.key = "cdproberoot";
  		env.keylen = strlen(env.key);
  		sprintf(devnum, "%d", (int) probedev);
  		env.val = devnum;
  		env.vallen = strlen(devnum);
  		svrctl(MMSETPARAM, &env);
  	}

  	/* Open image device for RAM root. */
	if (dev_open(image_dev, FS_PROC_NR, R_BIT) != OK)
		panic(__FILE__,"Cannot open RAM image device", NO_NUM);

	/* Get size of RAM disk image from the super block. */
	sp = &super_block[0];
	sp->s_dev = image_dev;
	if (read_super(sp) != OK) 
		panic(__FILE__,"Bad RAM disk image FS", NO_NUM);

	lcount = sp->s_zones << sp->s_log_zone_size;	/* # blks on root dev*/

	/* Stretch the RAM disk file system to the boot parameters size, but
	 * no further than the last zone bit map block allows.
	 */
	if (ram_size_kb*1024 < lcount*sp->s_block_size)
		ram_size_kb = lcount*sp->s_block_size/1024;
	fsmax = (u32_t) sp->s_zmap_blocks * CHAR_BIT * sp->s_block_size;
	fsmax = (fsmax + (sp->s_firstdatazone-1)) << sp->s_log_zone_size;
	if (ram_size_kb*1024 > fsmax*sp->s_block_size)
		ram_size_kb = fsmax*sp->s_block_size/1024;
  }

  /* Tell RAM driver how big the RAM disk must be. */
  m_out.m_type = DEV_IOCTL;
  m_out.PROC_NR = FS_PROC_NR;
  m_out.DEVICE = RAM_DEV;
  m_out.REQUEST = MIOCRAMSIZE;			/* I/O control to use */
  m_out.POSITION = (ram_size_kb * 1024);	/* request in bytes */
  if ((s=sendrec(MEM_PROC_NR, &m_out)) != OK)
  	panic("FS","sendrec from MEM failed", s);
  else if (m_out.REP_STATUS != OK) {
  	/* Report and continue, unless RAM disk is required as root FS. */
  	if (root_dev != DEV_RAM) {
  		report("FS","can't set RAM disk size", m_out.REP_STATUS);
  		return;
  	} else {
		panic(__FILE__,"can't set RAM disk size", m_out.REP_STATUS);
  	}
  }

#if ENABLE_CACHE2
  /* The RAM disk is a second level block cache while not otherwise used. */
  init_cache2(ram_size);
#endif

  /* See if we must load the RAM disk image, otherwise return. */
  if (root_dev != DEV_RAM)
  	return;

  /* Copy the blocks one at a time from the image to the RAM disk. */
  printf("Loading RAM disk onto /dev/ram:\33[23CLoaded:    0 KB");

  inode[0].i_mode = I_BLOCK_SPECIAL;	/* temp inode for rahead() */
  inode[0].i_size = LONG_MAX;
  inode[0].i_dev = image_dev;
  inode[0].i_zone[0] = image_dev;

  block_size_ram = get_block_size(DEV_RAM);
  block_size_image = get_block_size(image_dev);

  /* RAM block size has to be a multiple of the root image block
   * size to make copying easier.
   */
  if (block_size_image % block_size_ram) {
  	printf("\nram block size: %d image block size: %d\n", 
  		block_size_ram, block_size_image);
  	panic(__FILE__, "ram disk block size must be a multiple of "
  		"the image disk block size", NO_NUM);
  }

  /* Loading blocks from image device. */
  for (b = 0; b < (block_t) lcount; b++) {
  	int rb, factor;
	bp = rahead(&inode[0], b, (off_t)block_size_image * b, block_size_image);
	factor = block_size_image/block_size_ram;
  	for(rb = 0; rb < factor; rb++) {
		bp1 = get_block(root_dev, b * factor + rb, NO_READ);
		memcpy(bp1->b_data, bp->b_data + rb * block_size_ram,
			(size_t) block_size_ram);
		bp1->b_dirt = DIRTY;
		put_block(bp1, FULL_DATA_BLOCK);
	}
	put_block(bp, FULL_DATA_BLOCK);
	if (b % 11 == 0)
	printf("\b\b\b\b\b\b\b\b\b%6ld KB", ((long) b * block_size_image)/1024L);
  }

  /* Commit changes to RAM so dev_io will see it. */
  do_sync();

  printf("\rRAM disk of %u KB loaded onto /dev/ram.", (unsigned) ram_size_kb);
  if (root_dev == DEV_RAM) printf(" Using RAM disk as root FS.");
  printf("  \n");

  /* Invalidate and close the image device. */
  invalidate(image_dev);
  dev_close(image_dev);

  /* Resize the RAM disk root file system. */
  if (dev_io(DEV_READ, root_dev, FS_PROC_NR,
  	sbbuf, SUPER_BLOCK_BYTES, MIN_BLOCK_SIZE, 0) != MIN_BLOCK_SIZE) {
  	printf("WARNING: ramdisk read for resizing failed\n");
  }
  dsp = (struct super_block *) sbbuf;
  if (dsp->s_magic == SUPER_V3)
  	ramfs_block_size = dsp->s_block_size;
  else
  	ramfs_block_size = STATIC_BLOCK_SIZE;
  zones = (ram_size_kb * 1024 / ramfs_block_size) >> sp->s_log_zone_size;

  dsp->s_nzones = conv2(sp->s_native, (u16_t) zones);
  dsp->s_zones = conv4(sp->s_native, zones);
  if (dev_io(DEV_WRITE, root_dev, FS_PROC_NR,
  	sbbuf, SUPER_BLOCK_BYTES, MIN_BLOCK_SIZE, 0) != MIN_BLOCK_SIZE) {
  	printf("WARNING: ramdisk write for resizing failed\n");
  }
}

/*===========================================================================*
 *				load_super				     *
 *===========================================================================*/
PRIVATE void load_super(super_dev)
dev_t super_dev;			/* place to get superblock from */
{
  int bad;
  register struct super_block *sp;
  register struct inode *rip;

  /* Initialize the super_block table. */
  for (sp = &super_block[0]; sp < &super_block[NR_SUPERS]; sp++)
  	sp->s_dev = NO_DEV;

  /* Read in super_block for the root file system. */
  sp = &super_block[0];
  sp->s_dev = super_dev;

  /* Check super_block for consistency. */
  bad = (read_super(sp) != OK);
  if (!bad) {
	rip = get_inode(super_dev, ROOT_INODE);	/* inode for root dir */
	if ( (rip->i_mode & I_TYPE) != I_DIRECTORY || rip->i_nlinks < 3) bad++;
  }
  if (bad) panic(__FILE__,"Invalid root file system", NO_NUM);

  sp->s_imount = rip;
  dup_inode(rip);
  sp->s_isup = rip;
  sp->s_rd_only = 0;
  return;
}
