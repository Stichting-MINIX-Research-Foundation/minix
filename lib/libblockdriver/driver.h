#ifndef _BLOCKDRIVER_DRIVER_H
#define _BLOCKDRIVER_DRIVER_H

#define SINGLE_THREAD	(0)				/* single-thread ID */
#define MAIN_THREAD	(BLOCKDRIVER_MT_MAX_WORKERS)	/* main thread ID */

_PROTOTYPE( void blockdriver_handle_notify, (struct blockdriver *bdp,
	message *m_ptr) );
_PROTOTYPE( int blockdriver_handle_request, (struct blockdriver *bdp,
	message *m_ptr, thread_id_t thread) );
_PROTOTYPE( void blockdriver_reply, (message *m_ptr, int ipc_status,
	int reply) );

#endif /* _BLOCKDRIVER_DRIVER_H */
