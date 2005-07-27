/*
inet/sr_int.h

SR internals

Created:	Aug 2004 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#define FD_NR			(16*IP_PORT_MAX)

typedef struct sr_fd
{
	int srf_flags;
	int srf_fd;
	int srf_port;
	int srf_select_proc;
	sr_open_t srf_open;
	sr_close_t srf_close;
	sr_write_t srf_write;
	sr_read_t srf_read;
	sr_ioctl_t srf_ioctl;
	sr_cancel_t srf_cancel;
	sr_select_t srf_select;
	mq_t *srf_ioctl_q, *srf_ioctl_q_tail;
	mq_t *srf_read_q, *srf_read_q_tail;
	mq_t *srf_write_q, *srf_write_q_tail;
	event_t srf_ioctl_ev;
	event_t srf_read_ev;
	event_t srf_write_ev;
} sr_fd_t;

#	define SFF_FREE		  0x00
#	define SFF_MINOR	  0x01
#	define SFF_INUSE	  0x02
#define SFF_BUSY		  0x1C
#	define SFF_IOCTL_IP	  0x04
#	define SFF_READ_IP	  0x08
#	define SFF_WRITE_IP	  0x10
#define SFF_SUSPENDED	0x1C0
#	define SFF_IOCTL_SUSP	  0x40
#	define SFF_READ_SUSP	  0x80
#	define SFF_WRITE_SUSP	 0x100
#define SFF_IOCTL_FIRST		 0x200
#define SFF_READ_FIRST		 0x400
#define SFF_WRITE_FIRST		 0x800
#define SFF_SELECT_R		0x1000
#define SFF_SELECT_W		0x2000
#define SFF_SELECT_X		0x4000

EXTERN sr_fd_t sr_fd_table[FD_NR];

/*
 * $PchId: sr_int.h,v 1.2 2005/06/28 14:28:17 philip Exp $
 */
