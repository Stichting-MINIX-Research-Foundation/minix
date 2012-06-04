#include "devman.h"
#include "proto.h"


static struct devman_device*devman_dev_add_child(struct devman_device
	*parent, struct devman_device_info *devinf);
static struct devman_device *_find_dev(struct devman_device *dev, int
	dev_id);
static int devman_dev_add_info(struct devman_device *dev, struct
	devman_device_info_entry *entry, char *buf);
static int devman_event_read(char **ptr, size_t *len,off_t offset, void
	*data);

static int devman_del_device(struct devman_device *dev);

static int next_device_id = 1;

static struct inode_stat default_dir_stat = {
	/* .mode  = */ S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH,
	/* .uid   = */ 0,
	/* .gid   = */ 0,
	/* .size  = */ 0,
	/* .dev   = */ NO_DEV,
};

static struct inode_stat default_file_stat = {
	/* .mode  = */ S_IFREG | S_IRUSR | S_IRGRP | S_IROTH,
	/* .uid   = */ 0,
	/* .gid   = */ 0,
	/* .size  = */ 0x1000,
	/* .dev   = */ NO_DEV,
};


static struct devman_device root_dev; 
static struct devman_event_inode event_inode_data = {
	 TAILQ_HEAD_INITIALIZER(event_inode_data.event_queue),
};
static struct devman_inode event_inode;

/*===========================================================================*
 *           devman_generate_path                                            *
 *===========================================================================*/
static int 
devman_generate_path(char* buf, int len, struct devman_device *dev)
{
	int res =0;
 	const char * name = ".";
	const char * sep = "/";
	
	if (dev != NULL) {
		res = devman_generate_path(buf, len, dev->parent);
		if (res != 0) {
			return res;
		}
		name = get_inode_name(dev->inode.inode);
	} else {
	}
	
	/* does it fit? */
	if (strlen(buf) + strlen(name) + strlen(sep) + 1 > len) {
		return ENOMEM;
	}

	strcat(buf, name);
	strcat(buf, sep);
	
	return 0;		
}

/*===========================================================================*
 *          devman_device_add_event                                          *
 *===========================================================================*/
static void 
devman_device_add_event(struct devman_device* dev)
{
	struct devman_event * event;
	char buf[12]; /* this fits the device ID " 0xXXXXXXXX" */
	int res;
	
	event = malloc(sizeof(struct devman_event));

	if (event == NULL) {
		panic("devman_device_remove_event: out of memory\n");
	}

	memset(event, 0, sizeof(*event));

	strncpy(event->data, ADD_STRING, DEVMAN_STRING_LEN - 1);

	res = devman_generate_path(event->data, DEVMAN_STRING_LEN - 11 , dev);
		
	if (res) {
		panic("devman_device_add_event: "
		    "devman_generate_path failed: (%d)\n", res);
	}

	snprintf(buf, 12, " 0x%08x", dev->dev_id);
	strcat(event->data,buf);

	TAILQ_INSERT_HEAD(&event_inode_data.event_queue, event, events);
}

/*===========================================================================*
 *          devman_device_remove_event                                       *
 *===========================================================================*/
static void 
devman_device_remove_event(struct devman_device* dev)
{
	struct devman_event * event;
	char buf[12]; /* this fits the device ID " 0xXXXXXXXX" */
	int res;
	
	event = malloc(sizeof(struct devman_event));

	if (event == NULL) {
		panic("devman_device_remove_event: out of memory\n");
	}

	memset(event, 0, sizeof(*event));

	strncpy(event->data, REMOVE_STRING, DEVMAN_STRING_LEN - 1);

	res = devman_generate_path(event->data, DEVMAN_STRING_LEN-11, dev);
	
	if (res) {
		panic("devman_device_remove_event: "
		    "devman_generate_path failed: (%d)\n", res);
	}

	snprintf(buf, 12, " 0x%08x", dev->dev_id);
	strcat(event->data,buf);


	TAILQ_INSERT_HEAD(&event_inode_data.event_queue, event, events);
}

/*===========================================================================*
 *          devman_event_read                                                *
 *===========================================================================*/
static int
devman_event_read(char **ptr, size_t *len,off_t offset, void *data)
{
	struct devman_event *ev = NULL;
	struct devman_event_inode *n;
	static int eof = 0;	
	
	if (eof) {
		*len=0;
		eof = 0;
		return 0;
	}
	n = (struct devman_event_inode *) data;
	
	if (!TAILQ_EMPTY(&n->event_queue)) {
		ev = TAILQ_LAST(&n->event_queue, event_head);
	}

	buf_init(offset, *len);
	if (ev != NULL) {
		buf_printf("%s", ev->data);
		/* read all? */
		if (*len + offset >= strlen(ev->data)) {
			TAILQ_REMOVE(&n->event_queue, ev, events);
			free(ev);
			eof = 1;
		}
	}

	*len = buf_get(ptr);
	
	return 0;
}

/*===========================================================================*
 *          devman_static_info_read                                          *
 *===========================================================================*/
static int
devman_static_info_read(char **ptr, size_t *len, off_t offset, void *data)
{
	struct devman_static_info_inode *n;

	n = (struct devman_static_info_inode *) data;

	buf_init(offset, *len);
	buf_printf("%s\n", n->data);
	*len = buf_get(ptr);
	return 0;
}

/*===========================================================================*
 *           devman_init_devices                                             *
 *===========================================================================*/
void devman_init_devices() 
{
	event_inode.data   =  &event_inode_data;
	event_inode.read_fn =  devman_event_read;

	root_dev.dev_id =    0;
	root_dev.major  =   -1;
	root_dev.owner  =    0;
	root_dev.parent = NULL;

	root_dev.inode.inode= 
		add_inode(get_root_inode(), "devices",
		    NO_INDEX, &default_dir_stat, 0, &root_dev.inode);

	event_inode.inode= 
		add_inode(get_root_inode(), "events",
		    NO_INDEX, &default_file_stat, 0, &event_inode);

	TAILQ_INIT(&root_dev.children);
	TAILQ_INIT(&root_dev.infos);
}


/*===========================================================================*
 *           do_reply                                                        *
 *===========================================================================*/
static void do_reply(message *msg, int res)
{
	msg->m_type = DEVMAN_REPLY;
	msg->DEVMAN_RESULT = res;
	send(msg->m_source, msg);
}

/*===========================================================================*
 *           do_add_device                                                   *
 *===========================================================================*/
int do_add_device(message *msg)
{
	endpoint_t ep = msg->m_source;
	int res;
	struct devman_device *dev;
	struct devman_device *parent;
	struct devman_device_info *devinf = NULL;
	
	devinf = malloc(msg->DEVMAN_GRANT_SIZE);

	if (devinf == NULL) {
		res = ENOMEM;
		do_reply(msg, res);
		return 0;
	}
	
	res = sys_safecopyfrom(ep, msg->DEVMAN_GRANT_ID,
	          0, (vir_bytes) devinf, msg->DEVMAN_GRANT_SIZE);

	if (res != OK) {
		res = EINVAL;
		free(devinf);
		do_reply(msg, res);
		return 0;
	}

	if ((parent = _find_dev(&root_dev, devinf->parent_dev_id))
		 == NULL) {
		res = ENODEV;
		free(devinf);
		do_reply(msg, res);
		return 0;
	}

	dev = devman_dev_add_child(parent, devinf);

	if (dev == NULL) {
		res = ENODEV;
		free(devinf);
		do_reply(msg, res);
		return 0;
	}

	dev->state = DEVMAN_DEVICE_UNBOUND;
	
	dev->owner = msg->m_source;

	msg->DEVMAN_DEVICE_ID = dev->dev_id;
		
	devman_device_add_event(dev);

	do_reply(msg, res);
	return 0;
}


/*===========================================================================*
 *           _find_dev                                                       *
 *===========================================================================*/
static struct devman_device *
_find_dev(struct devman_device *dev, int dev_id)
{
	struct devman_device *_dev;

	if(dev->dev_id == dev_id)
		return dev;

	TAILQ_FOREACH(_dev, &dev->children, siblings) {

		struct devman_device *t = _find_dev(_dev, dev_id);

		if (t !=NULL) {
			return t;
		}
	}

	return NULL;
}

/*===========================================================================*
 *           devman_find_dev                                                 *
 *===========================================================================*/
struct devman_device *devman_find_device(int dev_id)
{
	return _find_dev(&root_dev, dev_id);
}

/*===========================================================================*
 *           devman_dev_add_static_info                                      *
 *===========================================================================*/
static int 
devman_dev_add_static_info
(struct devman_device *dev, char * name, char *data)
{
	struct devman_inode *inode;
	struct devman_static_info_inode *st_inode;
	
	
	st_inode          = malloc(sizeof(struct devman_static_info_inode));
	st_inode->dev     = dev;
	
	strncpy(st_inode->data, data, DEVMAN_STRING_LEN);
	/* if string is longer it's truncated */
	st_inode->data[DEVMAN_STRING_LEN-1] = 0;

	inode          = malloc (sizeof(struct devman_inode));
	inode->data    = st_inode;
	inode->read_fn = devman_static_info_read;
	
	inode->inode = add_inode(dev->inode.inode, name,
			NO_INDEX, &default_file_stat, 0, inode);
	
	/* add info to info_list */
	TAILQ_INSERT_HEAD(&dev->infos, inode, inode_list);

	return 0;
}

/*===========================================================================*
 *           devman_dev_add_child                                            *
 *===========================================================================*/
static struct devman_device* 
devman_dev_add_child
(struct devman_device *parent, struct devman_device_info *devinf)
{
	int i;
	char * buffer = (char *) (devinf);
	char tmp_buf[128];
	struct devman_device_info_entry *entries;

	/* create device */
	struct devman_device * dev = malloc(sizeof(struct devman_device));
	if (dev == NULL) {
		panic("devman_dev_add_child: out of memory\n");
	}


	if (parent == NULL) {
		free(dev);
		return NULL;
	}
	
	dev->ref_count = 1;

	/* set dev_info */
	dev->parent   = parent;
	dev->info = devinf;
    
    dev->dev_id = next_device_id++;

	dev->inode.inode = 
		add_inode(parent->inode.inode, buffer + devinf->name_offset,
		    NO_INDEX, &default_dir_stat, 0, &dev->inode);
		
	TAILQ_INIT(&dev->children);
	TAILQ_INIT(&dev->infos);

	/* create information inodes */
	entries = (struct devman_device_info_entry *) 
		(buffer + sizeof(struct devman_device_info));
	
	for (i = 0; i < devinf->count ; i++) {
		devman_dev_add_info(dev, &entries[i], buffer);
	}

	/* make device ID accessible to user land */
	snprintf(tmp_buf, DEVMAN_STRING_LEN, "%d",dev->dev_id);
	devman_dev_add_static_info(dev, "devman_id", tmp_buf);

	TAILQ_INSERT_HEAD(&parent->children, dev, siblings);
	
	devman_get_device(parent);

	/* FUTURE TODO: create links(BUS, etc) */
	return dev;
}

/*===========================================================================*
 *           devman_dev_add_info                                             *
 *===========================================================================*/
static int 
devman_dev_add_info
(struct devman_device *dev, struct devman_device_info_entry *entry, char *buf)
{
	switch(entry->type) {
	
	case DEVMAN_DEVINFO_STATIC:
			return devman_dev_add_static_info(dev, 
			    buf + entry->name_offset, buf + entry->data_offset);

	case DEVMAN_DEVINFO_DYNAMIC:
		/* TODO */
		/* fall through */
	default:
		return -1;
	}
}

/*===========================================================================*
 *           do_del_device                                                   *
 *===========================================================================*/
int do_del_device(message *msg)
{
	int dev_id = msg->DEVMAN_DEVICE_ID;
	
	int res=0;
	
	/* only parrent is allowed to add devices */
	struct devman_device *dev = _find_dev(&root_dev, dev_id);
	
	if (dev == NULL )  {
		printf("devman: no dev with id %d\n",dev_id);
		res = ENODEV;
	}

#if 0
	if  (dev->parent->owner != ep) {
		res = EPERM;
	}
#endif 

	if (!res) {
		devman_device_remove_event(dev);
		if (dev->state == DEVMAN_DEVICE_BOUND) {
			dev->state = DEVMAN_DEVICE_ZOMBIE;
		}
		devman_put_device(dev);
	}

	do_reply(msg, res);

	return 0;
}

/*===========================================================================*
 *           devman_get_device                                               *
 *===========================================================================*/
void devman_get_device(struct devman_device *dev)
{
	if (dev == NULL || dev == &root_dev) {
		return;
	}
	dev->ref_count++;
}

/*===========================================================================*
 *           devman_put_device                                               *
 *===========================================================================*/
void devman_put_device(struct devman_device *dev)
{
	if (dev == NULL || dev == &root_dev ) {
		return;
	}
	dev->ref_count--;
	if (dev->ref_count == 0) {
		devman_del_device(dev);
	}
}

/*===========================================================================*
 *           devman_del_device                                               *
 *===========================================================================*/
static int devman_del_device(struct devman_device *dev)
{
	/* does device have children -> error */
	/* evtl. remove links */

	/* free devinfo inodes */
	struct devman_inode *inode, *_inode;
	
	TAILQ_FOREACH_SAFE(inode, &dev->infos, inode_list, _inode) {
		
		delete_inode(inode->inode);
		
		TAILQ_REMOVE(&dev->infos, inode, inode_list);
		
		if (inode->data) {
			free(inode->data);
		}
		
		free(inode);
	}

	/* free device inode */
	delete_inode(dev->inode.inode);

	/* remove from parent */
	TAILQ_REMOVE(&dev->parent->children, dev, siblings);

	devman_put_device(dev->parent);

	/* free devinfo */
	free(dev->info);
		
	/* free device */
	free(dev);
	return 0;
}
