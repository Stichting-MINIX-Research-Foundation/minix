/*
sr.h

Copyright 1995 Philip Homburg
*/

#ifndef SR_H
#define SR_H

#define MAX_IOCTL_S	512

#define SR_CANCEL_IOCTL	1
#define SR_CANCEL_READ	2
#define SR_CANCEL_WRITE	3

#define SR_SELECT_READ		0x01
#define SR_SELECT_WRITE		0x02
#define SR_SELECT_EXCEPTION	0x04
#define SR_SELECT_POLL		0x10

/* Forward struct declarations */

struct acc;

/* prototypes */

typedef int  (*sr_open_t) ARGS(( int port, int srfd,
	get_userdata_t get_userdata, put_userdata_t put_userdata,
	put_pkt_t put_pkt, select_res_t select_res ));
typedef void (*sr_close_t) ARGS(( int fd ));
typedef int (*sr_read_t) ARGS(( int fd, size_t count ));
typedef int (*sr_write_t) ARGS(( int fd, size_t count ));
typedef int  (*sr_ioctl_t) ARGS(( int fd, ioreq_t req ));
typedef int  (*sr_cancel_t) ARGS(( int fd, int which_operation ));
typedef int  (*sr_select_t) ARGS(( int fd, unsigned operations ));

void sr_init ARGS(( void  ));
void sr_add_minor ARGS(( int minor, int port, sr_open_t openf,
	sr_close_t closef, sr_read_t sr_read, sr_write_t sr_write,
	sr_ioctl_t ioctlf, sr_cancel_t cancelf, sr_select_t selectf ));

#endif /* SR_H */

/* Track TCP connections back into sr (for lsof, identd, etc.) */
EXTERN sr_cancel_t tcp_cancel_f;

/*
 * $PchId: sr.h,v 1.9 2005/06/28 14:19:51 philip Exp $
 */
