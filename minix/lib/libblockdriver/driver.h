#ifndef _BLOCKDRIVER_DRIVER_H
#define _BLOCKDRIVER_DRIVER_H

void blockdriver_process_on_thread(struct blockdriver *bdp, message *m_ptr,
	int ipc_status, thread_id_t thread);
void blockdriver_reply(message *m_ptr, int ipc_status, int reply);

#endif /* _BLOCKDRIVER_DRIVER_H */
