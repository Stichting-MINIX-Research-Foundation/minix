#include "devman.h"
#include "proto.h"

/*****************************************************************************
 *    do_bind_device                                                         *
 ****************************************************************************/
int do_bind_device(message *m)
{
	struct devman_device *dev;
	int res;
	endpoint_t src = m->m_source;

	/* check if msg comes from RS */
	if (src != RS_PROC_NR) {
		m->DEVMAN_RESULT = EPERM;
		printf("[W] could bind message from somebody else than RS\n");
		
		return 0;
	} else {
		/* get the device */
		dev = devman_find_device(m->DEVMAN_DEVICE_ID);
		/* bind device at device provider*/
		if (dev != NULL) {
			m->m_type = DEVMAN_BIND;
			/* ...device ID and endpoint is still set */

#ifdef DEBUG
			printf("devman: bind call to %d for dev %d\n",
			    dev->owner, m->DEVMAN_DEVICE_ID);
#endif
			
			res = ipc_sendrec(dev->owner, m);
			if (res != OK) {
				printf("[W] devman.do_bind_device(): could not send "
				       "message to device owner (%d)\n", res);
				m->DEVMAN_RESULT= res;
			} else if (m->DEVMAN_RESULT != OK) {
				printf("[W] devman.do_bind_device(): driver could"
				       " not bind device (%ld)\n", m->DEVMAN_RESULT);
			} else {
				dev->state = DEVMAN_DEVICE_BOUND;
				devman_get_device(dev);
			}
		} else {
			m->DEVMAN_RESULT = ENODEV;
		}
		m->m_type = DEVMAN_REPLY;
		ipc_send(RS_PROC_NR, m);
	}
	return 0;
}

/*****************************************************************************
 *    do_unbind_device                                                       *
 ****************************************************************************/
int do_unbind_device(message *m)
{
	struct devman_device *dev;
	int res;
	endpoint_t src = m->m_source;

	/* check if msg comes from RS */
	if (src != RS_PROC_NR) {
		m->DEVMAN_RESULT = EPERM;
		printf("[W] devman.do_unbind_device(): unbind message from somebody"
		       "else than RS (%d)\n", src);
		return 0;
	} else {
		/* get the device */
		dev = devman_find_device(m->DEVMAN_DEVICE_ID);
		/* bind device at device provider*/
		if (dev != NULL) {

			m->m_type = DEVMAN_UNBIND;
			/* ...device ID and endpoint is still set */
#ifdef DEBUG
			printf("devman: unbind call to %d for dev %d\n",
			    dev->owner, m->DEVMAN_DEVICE_ID);
#endif
			res = ipc_sendrec(dev->owner, m);
			if (res != OK) {
				printf("[W] devman.do_unbind_device(): could not send "
				       "message to device owner (%d)\n", res);
				m->DEVMAN_RESULT= res;
			} else if (m->DEVMAN_RESULT != OK && m->DEVMAN_RESULT != 19) {
				/* device drive deleted device already? */
				printf("[W] devman.do_unbind_device(): driver could"
				       " not unbind device (%ld)\n", m->DEVMAN_RESULT);
			} else { 
				if (dev->state != DEVMAN_DEVICE_ZOMBIE) {
					dev->state = DEVMAN_DEVICE_UNBOUND;
				}
				devman_put_device(dev);
				m->DEVMAN_RESULT = OK;
			}
		} else {
			/* this might be the case, but perhaps its better to keep 
			   the device in the db as long a driver is bound to it*/
			m->DEVMAN_RESULT = ENODEV;
		}
		m->m_type = DEVMAN_REPLY;
		ipc_send(RS_PROC_NR, m);
	}
	return 0;
}
