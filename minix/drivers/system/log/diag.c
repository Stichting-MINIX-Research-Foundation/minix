/* This file handle diagnostic output that is sent to the LOG driver. Output
 * can be either from the kernel, or from other system processes. Output from
 * system processes is also routed through the kernel. The kernel notifies
 * this driver with a SIGKMESS signal if any messages are available.
 *
 * Changes:
 *	21 July 2005:	Created  (Jorrit N. Herder)
 */

#include "log.h"

#include <assert.h>

/*==========================================================================*
 *				do_new_kmess				    *
 *==========================================================================*/
void do_new_kmess(void)
{
/* Notification for a new kernel message. */
  static struct kmessages *kmess;		/* entire kmess structure */
  static char print_buf[_KMESS_BUF_SIZE];	/* copy new message here */
  int i, r, next, bytes;
  static int prev_next = 0;

  kmess = get_minix_kerninfo()->kmessages;

  /* Print only the new part. Determine how many new bytes there are with 
   * help of the current and previous 'next' index. Note that the kernel
   * buffer is circular. This works fine if less than KMESS_BUF_SIZE bytes
   * are new data; else we miss % KMESS_BUF_SIZE here. Obtain 'next' only
   * once, since we are operating on shared memory here.
   * Check for size being positive, the buffer might as well be emptied!
   */
  next = kmess->km_next;
  bytes = ((next + _KMESS_BUF_SIZE) - prev_next) % _KMESS_BUF_SIZE;
  if (bytes > 0) {
      r= prev_next;				/* start at previous old */ 
      i=0;
      while (bytes > 0) {			
          print_buf[i] = kmess->km_buf[(r%_KMESS_BUF_SIZE)];
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
  prev_next = next;
}
