/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

char rpc_buf[RPC_BUF_SIZE];
char *rpc_ptr;

static struct channel rpc_chan;

/*===========================================================================*
 *				rpc_open				     *
 *===========================================================================*/
int rpc_open(void)
{
/* Open a HGFS RPC backdoor channel to the VMware host, and make sure that it
 * is working. Return OK upon success, or a negative error code otherwise; in
 * particular, return EAGAIN if shared folders are disabled.
 */
  int r;

  if ((r = channel_open(&rpc_chan, CH_OUT)) != OK)
	return r;

  r = rpc_test();

  if (r != OK)
	channel_close(&rpc_chan);

  return r;
}

/*===========================================================================*
 *				rpc_query				     *
 *===========================================================================*/
int rpc_query(void)
{
/* Send a HGFS RPC query over the backdoor channel. Return OK upon success, or
 * a negative error code otherwise; EAGAIN is returned if shared folders are
 * disabled. In general, we make the assumption that the sender (= VMware)
 * speaks the protocol correctly. Hence, the callers of this function do not
 * check for lengths.
 */
  int r, len, err;

  len = RPC_LEN;

  /* A more robust version of this call could reopen the channel and
   * retry the request upon low-level failure.
   */
  r = channel_send(&rpc_chan, rpc_buf, len);
  if (r < 0) return r;

  r = channel_recv(&rpc_chan, rpc_buf, sizeof(rpc_buf));
  if (r < 0) return r;
  if (r < 2 || (len > 2 && r < 10)) return EIO;

  RPC_RESET;

  if (RPC_NEXT8 != '1') return EAGAIN;
  if (RPC_NEXT8 != ' ') return EAGAIN;

  if (len <= 2) return OK;

  RPC_ADVANCE(sizeof(u32_t)); /* Skip over id field. */
  err = RPC_NEXT32;

  return error_convert(err);
}

/*===========================================================================*
 *				rpc_test				     *
 *===========================================================================*/
int rpc_test(void)
{
/* Test whether HGFS communication is working. Return OK on success, EAGAIN if
 * shared folders are disabled, or another negative error code upon error.
 */

  RPC_RESET;
  RPC_NEXT8 = 'f';
  RPC_NEXT8 = ' ';

  return rpc_query();
}

/*===========================================================================*
 *				rpc_close				     *
 *===========================================================================*/
void rpc_close(void)
{
/* Close the HGFS RPC backdoor channel.
 */

  channel_close(&rpc_chan);
}
