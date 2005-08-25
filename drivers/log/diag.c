/* This file handle diagnostic output that is directly sent to the LOG driver.
 * This output can either be a kernel message (announced through a SYS_EVENT
 * with a SIGKMESS in the signal set) or output from another system process
 * (announced through a DIAGNOSTICS message).
 *
 * Changes:
 *	21 July 2005:	Created  (Jorrit N. Herder)
 */

#include <stdio.h>
#include <fcntl.h>

#include "log.h"
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"

/*==========================================================================*
 *				do_new_kmess				    *
 *==========================================================================*/
PUBLIC int do_new_kmess(m)
message *m;					/* notification message */
{
/* Notification for a new kernel message. */
  struct kmessages kmess;		/* entire kmess structure */
  char print_buf[KMESS_BUF_SIZE];	/* copy new message here */
  static int prev_next = 0;
  int bytes;
  int i, r;

  /* Try to get a fresh copy of the buffer with kernel messages. */
  if ((r=sys_getkmessages(&kmess)) != OK) {
  	report("LOG","couldn't get copy of kmessages", r);
  	return EDONTREPLY;
  }

  /* Print only the new part. Determine how many new bytes there are with 
   * help of the current and previous 'next' index. Note that the kernel
   * buffer is circular. This works fine if less then KMESS_BUF_SIZE bytes
   * is new data; else we miss % KMESS_BUF_SIZE here.  
   * Check for size being positive, the buffer might as well be emptied!
   */
  if (kmess.km_size > 0) {
      bytes = ((kmess.km_next + KMESS_BUF_SIZE) - prev_next) % KMESS_BUF_SIZE;
      r=prev_next;				/* start at previous old */ 
      i=0;
      while (bytes > 0) {			
          print_buf[i] = kmess.km_buf[(r%KMESS_BUF_SIZE)];
          bytes --;
          r ++;
          i ++;
      }
      /* Now terminate the new message and print it. */
      print_buf[i] = 0;
      printf("%s", print_buf);
      log_append(print_buf, i);
  }

  /* Almost done, store 'next' so that we can determine what part of the
   * kernel messages buffer to print next time a notification arrives.
   */
  prev_next = kmess.km_next;
  return EDONTREPLY;
}

/*===========================================================================*
 *				do_diagnostics				     *
 *===========================================================================*/
PUBLIC int do_diagnostics(message *m)
{
/* The LOG server handles all diagnostic messages from servers and device 
 * drivers. It forwards the message to the TTY driver to display it to the
 * user. It also saves a copy in a local buffer so that messages can be 
 * reviewed at a later time.
 */
  int result;
  int proc_nr; 
  vir_bytes src;
  int count;
  char c;
  int i = 0;
  static char diagbuf[10240];

  /* Forward the message to the TTY driver. Inform the TTY driver about the
   * original sender, so that it knows where the buffer to be printed is.
   * The message type, DIAGNOSTICS, remains the same.
   */ 
  if ((proc_nr = m->DIAG_PROC_NR) == SELF)
      m->DIAG_PROC_NR = proc_nr = m->m_source;
  result = _sendrec(TTY_PROC_NR, m);

  /* Now also make a copy for the private buffer at the LOG server, so
   * that the messages can be reviewed at a later time.
   */
  src = (vir_bytes) m->DIAG_PRINT_BUF;
  count = m->DIAG_BUF_COUNT; 
  while (count > 0 && i < sizeof(diagbuf)-1) {
      if (sys_datacopy(proc_nr, src, SELF, (vir_bytes) &c, 1) != OK) 
          break;		/* stop copying on error */
      src ++;
      count --;
      diagbuf[i++] = c;
  }
  log_append(diagbuf, i);

  return result;
}
