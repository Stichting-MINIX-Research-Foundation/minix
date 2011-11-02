#ifndef _DRIVER_MQ_H
#define _DRIVER_MQ_H

#define DRIVER_MQ_SINGLE	0	/* thread ID for single-threading */

_PROTOTYPE( void driver_mq_init, (void) );
_PROTOTYPE( int driver_mq_enqueue, (thread_id_t thread_id, const message *mess,
	int ipc_status) );
_PROTOTYPE( int driver_mq_dequeue, (thread_id_t thread_id, message *mess,
	int *ipc_status) );

#endif /* _DRIVER_MQ_H */
