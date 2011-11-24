#ifndef _BLOCKDRIVER_MQ_H
#define _BLOCKDRIVER_MQ_H

_PROTOTYPE( void mq_init, (void) );
_PROTOTYPE( int mq_enqueue, (thread_id_t thread_id, const message *mess,
	int ipc_status) );
_PROTOTYPE( int mq_dequeue, (thread_id_t thread_id, message *mess,
	int *ipc_status) );

#endif /* _BLOCKDRIVER_MQ_H */
