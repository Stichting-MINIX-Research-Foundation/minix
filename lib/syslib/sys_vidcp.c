#include "syslib.h"

/*===========================================================================*
 *                              sys_vidcopy			     	     *
 *===========================================================================*/
PUBLIC int sys_vidcopy(request, src_mem, src_vid, dst_vid, count)
int request;			/* MEM_VID_COPY or VID_VID_COPY */
u16_t *src_mem;			/* source address in memory */
unsigned src_vid;		/* source address in video memory */
unsigned dst_vid;		/* destination address in video */
unsigned count;			/* number of words to copy */
{
/* Console wants to display something on the screen. */
  message m;
  m.VID_REQUEST = request;
  m.VID_SRC_ADDR = src_mem;
  m.VID_SRC_OFFSET = src_vid;
  m.VID_DST_OFFSET = dst_vid;
  m.VID_CP_COUNT = count;
  return(_taskcall(SYSTASK, SYS_VIDCOPY, &m));
}

