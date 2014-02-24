#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <minix/com.h>
#include <minix/ipc.h>
#include <minix/const.h>
#include <minix/devman.h>
#include <minix/safecopies.h>
#include <minix/sysutil.h>
#include <minix/ds.h>

#include "local.h"

static endpoint_t devman_ep;

static int save_string(char *buffer, char *src, size_t *offset);

static TAILQ_HEAD(devlist_head, devman_dev) dev_list;

/****************************************************************************
 *     save_string                                                          *
 ***************************************************************************/
static int save_string(char *buffer, char *src, size_t *offset)
{
	unsigned old_offset = *offset;
	size_t len = strlen(src) + 1;
	memcpy(buffer + *offset, src, len);
	*offset += len;
	return old_offset;
}

/****************************************************************************
 *     serialize_dev                                                        *
 ***************************************************************************/
void *serialize_dev(struct devman_dev *dev, size_t *overall_size)
{
	/* determine size of serialized version of dev */
	char *buffer;
	char *string_buffer;
	size_t string_buffer_offset;
	size_t count = 0;
	size_t size = sizeof(struct devman_device_info);
	size_t strings_size = strlen(dev->name) + 1;
	struct devman_device_info * serialized_dev;
	struct devman_device_info_entry *entry;
	struct devman_static_attribute *attribute;
	
	TAILQ_FOREACH(attribute, &dev->attrs, list) {
		strings_size += strlen(attribute->name) + 1;
		strings_size += strlen(attribute->data) + 1;
		size += sizeof(struct devman_device_info_entry);
		count++;
	}
	
	buffer = malloc(size + strings_size);

	if (buffer == NULL) {
		return NULL;
	}

	string_buffer = buffer;
	string_buffer_offset = size; /* strings start after
                                    devman_device_info and 
							   devman_device_info_entries */

	/* serialize device */
	serialized_dev = (struct devman_device_info *) buffer;

	serialized_dev->count         = count;
	serialized_dev->parent_dev_id = dev->parent_dev_id;
	serialized_dev->name_offset   = 
	    save_string(string_buffer, dev->name, &string_buffer_offset);
#if 0
	serialized_dev->bus           =  
	    save_string(string_buffer, dev->bus, &string_buffer_offset);
#endif	

	/* serialize entries */
	entry = 
	    (struct devman_device_info_entry *)
	    (buffer + sizeof(struct devman_device_info));
									  
	TAILQ_FOREACH(attribute, &dev->attrs, list) {
		entry->type = 0; /* TODO: use macro */
		entry->name_offset =
		    save_string(string_buffer, attribute->name, &string_buffer_offset);
		entry->data_offset =
		    save_string(string_buffer, attribute->data, &string_buffer_offset);
		entry++;
	}

	*overall_size = size + strings_size;

	return buffer;

}

/****************************************************************************
 *     devman_add_device                                                    *
 ***************************************************************************/
int devman_add_device(struct devman_dev *dev)
{
	message msg;
	int res;
	size_t grant_size;
	void *buf = serialize_dev(dev, &grant_size);

	cp_grant_id_t gid = 
	    cpf_grant_direct(devman_ep,(vir_bytes) buf,
		    grant_size, CPF_READ);
	
	/* prepare message */
	msg.m_type            = DEVMAN_ADD_DEV;
	msg.DEVMAN_GRANT_ID   = gid;
	msg.DEVMAN_GRANT_SIZE = grant_size;
		
	/* send message */
	res = ipc_sendrec(devman_ep, &msg);
	
	if (res != 0) {
		panic("devman_add_device: could not talk to devman: %d", res);
	}

	if (msg.m_type != DEVMAN_REPLY) {
		panic("devman_add_device: got illegal response from devman: %d",
		    msg.m_type);
	}
	
	if (msg.DEVMAN_RESULT != 0) {
		panic("devman_add_device: could add device: %ld",
		    msg.DEVMAN_RESULT);
	}

	/* store given dev_id to dev */
	dev->dev_id = msg.DEVMAN_DEVICE_ID;

	cpf_revoke(gid);

	free(buf);

	/* put device in list */
	TAILQ_INSERT_HEAD(&dev_list, dev, dev_list);	

	return 0;
}

/****************************************************************************
 *     devman_del_device                                                    *
 ***************************************************************************/
int devman_del_device(struct devman_dev *dev)
{
	message msg;
	int res;

	msg.m_type            = DEVMAN_DEL_DEV;
	msg.DEVMAN_DEVICE_ID   = dev->dev_id;

	res = ipc_sendrec(devman_ep, &msg);

	if (res != 0) {
		panic("devman_del_device: could not talk to devman: %d", res);
	}

	if (msg.m_type != DEVMAN_REPLY) {
		panic("devman_del_device: got illegal response from devman: %d",
		    msg.m_type);
	}
	
	if (msg.DEVMAN_RESULT != 0) {
		panic("devman_del_device: could delete device: %ld",
		    msg.DEVMAN_RESULT);
	}

	/* remove the device from list */
	TAILQ_REMOVE(&dev_list, dev, dev_list);	

	return 0;

}

/****************************************************************************
 *     devman_get_ep                                                        *
 ***************************************************************************/
endpoint_t devman_get_ep(void)
{
	return devman_ep;
}

/****************************************************************************
 *     devman_init                                                          *
 ***************************************************************************/
int devman_init(void)
{
	int res;

	/* get the endpoint of the HCD */
	res = ds_retrieve_label_endpt("devman", &devman_ep);

	if (res != 0) {
		panic("usb_init: ds_retrieve_label_endpt failed for 'devman': %d", res);
	}

	TAILQ_INIT(&dev_list);

	return res;
}

/****************************************************************************
 *     do_bind                                                              *
 ***************************************************************************/
static void do_bind(message *m)
{
	struct devman_dev *dev;
	int res;

	/* find device */
	TAILQ_FOREACH(dev, &dev_list, dev_list) {
		if (dev->dev_id == m->DEVMAN_DEVICE_ID) {
			if (dev->bind_cb) {
				res = dev->bind_cb(dev->data, m->DEVMAN_ENDPOINT);
				m->m_type = DEVMAN_REPLY;
				m->DEVMAN_RESULT = res;
				ipc_send(devman_ep, m);
				return;
			}
		}
	}
	m->m_type = DEVMAN_REPLY;
	m->DEVMAN_RESULT = ENODEV;
	ipc_send(devman_ep, m);
	return;
}

/****************************************************************************
 *     do_unbind                                                            *
 ***************************************************************************/
static void do_unbind(message *m)
{
	struct devman_dev *dev;
	int res;

	/* find device */
	TAILQ_FOREACH(dev, &dev_list, dev_list) {
		if (dev->dev_id == m->DEVMAN_DEVICE_ID) {
			if (dev->unbind_cb) {
				res = dev->unbind_cb(dev->data, m->DEVMAN_ENDPOINT);
				m->m_type = DEVMAN_REPLY;
				m->DEVMAN_RESULT = res;
				ipc_send(devman_ep, m);
				return;
			}
		}
	}
	m->m_type = DEVMAN_REPLY;
	m->DEVMAN_RESULT = ENODEV;
	ipc_send(devman_ep, m);
}

/****************************************************************************
 *     devman_handle_msg                                                    *
 ***************************************************************************/
int devman_handle_msg(message *m)
{
	/* make sure msg comes from devman server */
	if (m->m_source != devman_ep) {
		/* we don't honor requests from others by answering them */
		return 0;
	}
	switch (m->m_type) {
		case DEVMAN_BIND:
			do_bind(m);
			return 1;
		case DEVMAN_UNBIND:
			do_unbind(m);
			return 1;
		default:
			return 0;
	}
}
