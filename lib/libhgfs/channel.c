/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

#define CMD_XFER	0x1E		/* vmware backdoor transfer command */

enum {
  XFER_OPEN,				/* open transfer channel */
  XFER_SENDLEN,				/* specify length of data to send */
  XFER_SEND,				/* send data */
  XFER_RECVLEN,				/* get length of data to receive */
  XFER_RECV,				/* receive data */
  XFER_RECVACK,				/* acknowledge receipt of data */
  XFER_CLOSE				/* close transfer channel */
};

#define STATUS(p) (HIWORD((p)[2]) & 0xff)

/*===========================================================================*
 *				channel_open				     *
 *===========================================================================*/
int channel_open(ch, type)
struct channel *ch;			/* struct describing the new channel */
u32_t type;				/* channel type: CH_IN or CH_OUT */
{
/* Open a new backdoor channel. Upon success, the given channel structure will
 * be filled with information and can be used in subsequent channel calls.
 * Return OK on success, or a negative error code on error.
 */
  u32_t ptr[6];

  ptr[1] = type | 0x80000000;
  ptr[2] = MAKELONG(CMD_XFER, XFER_OPEN);

  backdoor(ptr);

  if ((STATUS(ptr) & 1) == 0) return EIO;

  ch->id = HIWORD(ptr[3]);
  ch->cookie1 = ptr[4];
  ch->cookie2 = ptr[5];

  return OK;
}

/*===========================================================================*
 *				channel_close				     *
 *===========================================================================*/
void channel_close(ch)
struct channel *ch;			/* backdoor channel to close */
{
/* Close a previously opened backdoor channel.
 */
  u32_t ptr[6];

  ptr[2] = MAKELONG(CMD_XFER, XFER_CLOSE);
  ptr[3] = MAKELONG(0, ch->id);
  ptr[4] = ch->cookie1;
  ptr[5] = ch->cookie2;

  backdoor(ptr);
}

/*===========================================================================*
 *				channel_send				     *
 *===========================================================================*/
int channel_send(ch, buf, len)
struct channel *ch;			/* backdoor channel to send to */
char *buf;				/* buffer to send data from */
int len;				/* size of the data to send */
{
/* Receive data over a backdoor channel. Return OK on success, or a negative
 * error code on error.
 */
  u32_t ptr[7];

  ptr[1] = len;
  ptr[2] = MAKELONG(CMD_XFER, XFER_SENDLEN);
  ptr[3] = MAKELONG(0, ch->id);
  ptr[4] = ch->cookie1;
  ptr[5] = ch->cookie2;

  backdoor(ptr);

  if ((STATUS(ptr) & 1) == 0) return EIO;

  if (len == 0) return OK;

  ptr[1] = MAKELONG(0, 1);
  ptr[2] = len;
  ptr[3] = MAKELONG(0, ch->id);
  ptr[4] = (u32_t)buf;
  ptr[5] = ch->cookie2;
  ptr[6] = ch->cookie1;

  backdoor_out(ptr);

  return OK;
}

/*===========================================================================*
 *				channel_recv				     *
 *===========================================================================*/
int channel_recv(ch, buf, max)
struct channel *ch;			/* backdoor channel to receive from */
char *buf;				/* buffer to receive data into */
int max;				/* size of the buffer */
{
/* Receive data on a backdoor channel. Return the number of bytes received, or
 * a negative error code on error.
 */
  u32_t ptr[7];
  int len;

  ptr[2] = MAKELONG(CMD_XFER, XFER_RECVLEN);
  ptr[3] = MAKELONG(0, ch->id);
  ptr[4] = ch->cookie1;
  ptr[5] = ch->cookie2;

  backdoor(ptr);

  if ((STATUS(ptr) & 0x81) == 0) return EIO;

  if ((len = ptr[1]) == 0 || (STATUS(ptr) & 3) == 1) return 0;

  if (len > max) return E2BIG;

  ptr[1] = MAKELONG(0, 1);
  ptr[2] = len;
  ptr[3] = MAKELONG(0, ch->id);
  ptr[4] = ch->cookie1;
  ptr[5] = (u32_t)buf;
  ptr[6] = ch->cookie2;

  backdoor_in(ptr);

  ptr[1] = 1;
  ptr[2] = MAKELONG(CMD_XFER, XFER_RECVACK);
  ptr[3] = MAKELONG(0, ch->id);
  ptr[4] = ch->cookie1;
  ptr[5] = ch->cookie2;

  backdoor(ptr);

  return len;
}
