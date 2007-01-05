
#include "inc.h"
#include <minix/dmap.h>
#include <minix/endpoint.h>

#include <minix/vfsif.h>
#include "fs.h"
#include "buf.h"
#include "inode.h"
#include "drivers.h"


/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m_in)			);

FORWARD _PROTOTYPE(void cch_check, (void)				);


/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(void)
{
/* This is the main routine of this service. The main loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  int who_e;				        /* caller */
  int error;
  message m;

  /* Initialize the server, then go to work. */
  init_server();	

  fs_m_in.m_type = FS_READY;

  if (sendrec(FS_PROC_NR, &fs_m_in) != OK) {
      printf("MFS(%d): Error sending login to VFS\n", SELF_E);
      return -1;
  }

  if (fs_m_in.m_type != REQ_READSUPER) {
      printf("MFS(%d): Invalid login reply\n", SELF_E);
      return -1;
  }
  else {
      fs_m_out.m_type = fs_readsuper();
      reply(FS_PROC_NR, &fs_m_out);
      if (fs_m_out.m_type != OK) return -1;
  }


  for (;;) {
      /* Wait for request message. */
      get_work(&fs_m_in);
      error = OK;

      who_e = fs_m_in.m_source;
      if (who_e != FS_PROC_NR) {
          if (who_e == 0) {
            /*
               printf("MFS(%d): MSG from PM\n", SELF_E); 
              error = 1;
              fs_m_out.m_type = error; 
              reply(who_e, &fs_m_out);
            */
          }
          continue;
      }

      req_nr = fs_m_in.m_type;

      if (req_nr < 0 || req_nr >= NREQS) {
          error = EINVAL; 
      }
      else {
          error = (*fs_call_vec[req_nr])();
	  /*cch_check();*/
      }

      fs_m_out.m_type = error; 
      reply(who_e, &fs_m_out);
      
		
      if (error == OK && rdahed_inode != NIL_INODE) {
          read_ahead(); /* do block read ahead */
      }

      /*
       * VFS asks RS to bring down the FS... */
      /*
      if (req_nr == REQ_UNMOUNT || 
              (req_nr == REQ_READSUPER && error != OK)) {
          printf("MFS(%d) exit() cachehit: %d cachemiss: %d\n", SELF_E,
			  inode_cache_hit, inode_cache_miss);
          return 0;
      }
      */
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
 *				init_server                                  *
 *===========================================================================*/
PRIVATE void init_server(void)
{
   int i;

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
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
PRIVATE void get_work(m_in)
message *m_in;				/* pointer to message */
{
    int s;				/* receive status */
    if (OK != (s = receive(ANY, m_in))) 	/* wait for message */
        panic("MFS","receive failed", s);
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PUBLIC void reply(who, m_out)
int who;	
message *m_out;                       	/* report result */
{
    if (OK != send(who, m_out))    /* send the message */
        printf("MFS(%d) was unable to send reply\n", SELF_E);
}

PRIVATE void cch_check(void) 
{
  int i;

  for (i = 0; i < NR_INODES; ++i) {
	  if (inode[i].i_count != cch[i] &&
		req_nr != REQ_OPEN && req_nr != REQ_GETNODE &&
		req_nr != REQ_PUTNODE &&
		req_nr != REQ_CLONE_OPCL && req_nr != REQ_READSUPER &&
		req_nr != REQ_MOUNTPOINT && req_nr != REQ_UNMOUNT &&
		req_nr != REQ_PIPE && req_nr != REQ_SYNC && 
		req_nr != REQ_LOOKUP)
printf("MFS(%d) inode(%d) cc: %d req_nr: %d\n",
	SELF_E, inode[i].i_num, inode[i].i_count - cch[i], req_nr);
	  
	  cch[i] = inode[i].i_count;
  }
}



