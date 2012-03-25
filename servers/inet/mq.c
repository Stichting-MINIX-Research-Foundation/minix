/*
inet/mq.c

Created:	Jan 3, 1992 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "mq.h"
#include "generic/assert.h"

THIS_FILE

#define MQ_SIZE		128

static mq_t mq_list[MQ_SIZE];
static mq_t *mq_freelist;

void mq_init()
{
	int i;

	mq_freelist= NULL;
	for (i= 0; i<MQ_SIZE; i++)
	{
		mq_list[i].mq_next= mq_freelist;
		mq_freelist= &mq_list[i];
		mq_list[i].mq_allocated= 0;
	}
}

mq_t *mq_get()
{
	mq_t *mq;

	mq= mq_freelist;
	assert(mq != NULL);

	mq_freelist= mq->mq_next;
	mq->mq_next= NULL;
	assert(mq->mq_allocated == 0);
	mq->mq_allocated= 1;
	return mq;
}

void mq_free(mq)
mq_t *mq;
{
	mq->mq_next= mq_freelist;
	mq_freelist= mq;
	assert(mq->mq_allocated == 1);
	mq->mq_allocated= 0;
}

/*
 * $PchId: mq.c,v 1.7 1998/10/23 20:10:47 philip Exp $
 */
