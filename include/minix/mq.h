/*
inet/mq.h

Created:	Jan 3, 1992 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef INET__MQ_H
#define INET__MQ_H

typedef struct mq
{
	message mq_mess;
	struct mq *mq_next;
	int mq_allocated;
} mq_t;

_PROTOTYPE( mq_t *mq_get, (void) );
_PROTOTYPE( void mq_free, (mq_t *mq) );
_PROTOTYPE( void mq_init, (void) );

#endif /* INET__MQ_H */

/*
 * $PchId: mq.h,v 1.4 1995/11/21 06:40:30 philip Exp $
 */
