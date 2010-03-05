/* This file handle diagnostic output that is directly sent to the LOG driver.
 * This output can either be a kernel message (announced through a SYS_EVENT
 * with a SIGKMESS in the signal set) or output from another system process
 * (announced through a DIAGNOSTICS message).
 *
 * Changes:
 *	21 July 2005:	Created  (Jorrit N. Herder)
 */

#include <stdio.h>
#include <minix/type.h>
#include <minix/safecopies.h>
#include <minix/sys_config.h>

#include "log.h"

/*==========================================================================*
 *				do_new_kmess				    *
 *==========================================================================*/
PUBLIC int do_new_kmess(from)
endpoint_t from;				/* who sent this message? */
{
/* Notification for a new kernel message. */
  static struct kmessages kmess;		/* entire kmess structure */
  static char print_buf[_KMESS_BUF_SIZE];	/* copy new message here */
  int bytes;
  int i, r;
  int *prev_nextp;

  static int kernel_prev_next = 0;
  static int tty_prev_next = 0;

  if (from == TTY_PROC_NR)
  {
	cp_grant_id_t gid;
	message mess;

	prev_nextp= &tty_prev_next;
	gid= cpf_grant_direct(TTY_PROC_NR, (vir_bytes)&kmess, sizeof(kmess),
		CPF_WRITE);
	if (gid == -1)
	{
		return EDONTREPLY;
	}

	/* Ask TTY driver for log output */
	mess.GETKM_GRANT= gid;
	mess.m_type = GET_KMESS_S;
	r= sendrec(TTY_PROC_NR, &mess);
	cpf_revoke(gid);

	if (r == OK) r= mess.m_type;
	if (r != OK)
	{
		printf("log: couldn't get copy of kmessages from TTY: %d\n", r);
		return EDONTREPLY;
	}
  }
  else
  {
	/* Try to get a fresh copy of the buffer with kernel messages. */
	if ((r=sys_getkmessages(&kmess)) != OK) {
		printf("log: couldn't get copy of kmessages: %d\n", r);
		return EDONTREPLY;
	}
	prev_nextp= &kernel_prev_next;
  }

  /* Print only the new part. Determine how many new bytes there are with 
   * help of the current and previous 'next' index. Note that the kernel
   * buffer is circular. This works fine if less then KMESS_BUF_SIZE bytes
   * is new data; else we miss % KMESS_BUF_SIZE here.  
   * Check for size being positive, the buffer might as well be emptied!
   */
  if (kmess.km_size > 0) {
      bytes = ((kmess.km_next + _KMESS_BUF_SIZE) - (*prev_nextp)) %
	_KMESS_BUF_SIZE;
      r= *prev_nextp;				/* start at previous old */ 
      i=0;
      while (bytes > 0) {			
          print_buf[i] = kmess.km_buf[(r%_KMESS_BUF_SIZE)];
          bytes --;
          r ++;
          i ++;
      }
      /* Now terminate the new message and save it in the log. */
      print_buf[i] = 0;
      log_append(print_buf, i);
  }

  /* Almost done, store 'next' so that we can determine what part of the
   * kernel messages buffer to print next time a notification arrives.
   */
  *prev_nextp = kmess.km_next;
  return EDONTREPLY;
}

/*===========================================================================*
 *				do_diagnostics				     *
 *===========================================================================*/
PUBLIC int do_diagnostics(message *m, int safe)
{
/* The LOG server handles all diagnostic messages from servers and device 
 * drivers. It forwards the message to the TTY driver to display it to the
 * user. It also saves a copy in a local buffer so that messages can be 
 * reviewed at a later time.
 */
  vir_bytes src;
  int count;
  char c;
  int i = 0, offset = 0;
  static char diagbuf[10240];

  /* Also make a copy for the private buffer at the LOG server, so
   * that the messages can be reviewed at a later time.
   */
  src = (vir_bytes) m->DIAG_PRINT_BUF_G;
  count = m->DIAG_BUF_COUNT; 
  while (count > 0 && i < sizeof(diagbuf)-1) {
      int r;
      if(safe) {
        r = sys_safecopyfrom(m->m_source, src, offset, (vir_bytes) &c, 1, D);
      } else {
        r = sys_datacopy(m->m_source, src+offset, SELF, (vir_bytes) &c, 1);
      }
      if(r != OK) break;
      offset ++;
      count --;
      diagbuf[i++] = c;
  }
  log_append(diagbuf, i);

  if(m->m_type == ASYN_DIAGNOSTICS_OLD) return EDONTREPLY;

  return OK;
}
