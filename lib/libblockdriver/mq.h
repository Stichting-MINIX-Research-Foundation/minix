#ifndef _BLOCKDRIVER_MQ_H
#define _BLOCKDRIVER_MQ_H

_PROTOTYPE( void mq_init, (void) );
_PROTOTYPE( int mq_enqueue, (device_id_t device_id, const message *mess,
	int ipc_status) );
_PROTOTYPE( int mq_dequeue, (device_id_t device_id, message *mess,
	int *ipc_status) );

#endif /* _BLOCKDRIVER_MQ_H */
