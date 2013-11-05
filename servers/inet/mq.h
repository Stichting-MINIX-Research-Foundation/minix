/*
inet/mq.h

Created:	Jan 3, 1992 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef INET__MQ_H
#define INET__MQ_H

#include <minix/chardriver.h>

typedef struct sr_req {
	enum {
		SRR_READ,
		SRR_WRITE,
		SRR_IOCTL
	}			srr_type;
	devminor_t		srr_minor;
	endpoint_t		srr_endpt;
	cp_grant_id_t		srr_grant;
	union {
		size_t		srr_size;	/* for SRR_READ, SRR_WRITE */
		unsigned long	srr_req;	/* for SRR_IOCTL */
	};
	int			srr_flags;
	cdev_id_t		srr_id;
} sr_req_t;

typedef struct mq
{
	sr_req_t mq_req;
	struct mq *mq_next;
	int mq_allocated;
} mq_t;

mq_t *mq_get(void);
void mq_free(mq_t *mq);
void mq_init(void);

#endif /* INET__MQ_H */

/*
 * $PchId: mq.h,v 1.4 1995/11/21 06:40:30 philip Exp $
 */
