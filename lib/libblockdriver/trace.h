#ifndef _BLOCKDRIVER_TRACE_H
#define _BLOCKDRIVER_TRACE_H

_PROTOTYPE( int trace_ctl, (dev_t minor, unsigned int request,
	endpoint_t endpt, cp_grant_id_t grant));

_PROTOTYPE( void trace_start, (thread_id_t thread_id, message *m_ptr));
_PROTOTYPE( void trace_setsize, (thread_id_t thread_id, size_t size));
_PROTOTYPE( void trace_finish, (thread_id_t thread_id, int r));

#endif /* _BLOCKDRIVER_TRACE_H */
