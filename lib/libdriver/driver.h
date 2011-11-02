#ifndef _DRIVER_DRIVER_H
#define _DRIVER_DRIVER_H

_PROTOTYPE( void driver_handle_notify, (struct driver *dp, message *m_ptr) );
_PROTOTYPE( int driver_handle_request, (struct driver *dp, message *m_ptr) );
_PROTOTYPE( void driver_reply, (int driver_type, message *m_ptr,
	int ipc_status, int reply) );

#endif /* _DRIVER_DRIVER_H */
