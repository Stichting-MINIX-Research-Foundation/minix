#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/devman.h>
#include <minix/usb.h>
#include <minix/sysutil.h>

#include "local.h"

#define CHECKOUTOFMEM(ptr) if(ptr == NULL) \
                               panic("Out of memory! (%s, line %d)", \
							     __FILE__, __LINE__)


static int (*bind_cb) (struct devman_usb_bind_cb_data *data, endpoint_t ep);
static int (*unbind_cb) (struct devman_usb_bind_cb_data *data, endpoint_t ep);

/****************************************************************************
 *    devman_usb_add_attr                                                   *
 ***************************************************************************/
static void 
devman_usb_add_attr
(struct devman_dev *dev, const char *name, const char *data)
{
	struct devman_static_attribute *attr = (struct devman_static_attribute *)
	    malloc(sizeof(struct devman_static_attribute));
	
	CHECKOUTOFMEM(attr);

	attr->name = malloc((strlen(name)+1)*sizeof(char));
	memcpy(attr->name, name, (strlen(name)+1));

	attr->data = malloc((strlen(data)+1)*sizeof(char));
	memcpy(attr->data, data, (strlen(data)+1));
	TAILQ_INSERT_TAIL(&dev->attrs, attr, list);
}

/****************************************************************************
 *    add_device_attributes                                                 *
 ***************************************************************************/
static void 
add_device_attributes
(struct devman_usb_dev *udev)
{
	int ret;
	char data[32];
	
	ret = snprintf(data,sizeof(data),"0x%02x",udev->desc->bDeviceClass);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(udev->dev, "bDeviceClass", data);
	
	ret = snprintf(data,sizeof(data),"0x%02x",udev->desc->bDeviceSubClass);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(udev->dev, "bDeviceSubClass", data);

	ret = snprintf(data,sizeof(data),"0x%02x",udev->desc->bDeviceProtocol);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(udev->dev, "bDeviceProtocol", data);

	ret = snprintf(data,sizeof(data),"0x%04x",UGETW(udev->desc->idVendor));
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(udev->dev, "idVendor", data);
	
	ret = snprintf(data,sizeof(data),"0x%04x",UGETW(udev->desc->idProduct));
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(udev->dev, "idProduct", data);	

	if (udev->product)
		devman_usb_add_attr(udev->dev, "Product", udev->product);
	if (udev->manufacturer)
		devman_usb_add_attr(udev->dev, "Manufacturer", udev->manufacturer);
	if (udev->serial)
		devman_usb_add_attr(udev->dev, "SerialNumber", udev->serial);
	devman_usb_add_attr(udev->dev, "dev_type", "USB_DEV");
}

/****************************************************************************
 *     add_interface_attributes                                             *
 ***************************************************************************/
static void 
add_interface_attributes
(struct devman_usb_interface *intf)
{
	int ret;
	char data[32];
	
	ret = snprintf(data,sizeof(data),"0x%02x",intf->desc->bInterfaceNumber);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(intf->dev, "bInterfaceNumber", data);

	ret = snprintf(data,sizeof(data),"0x%02x",intf->desc->bAlternateSetting);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(intf->dev, "bAlternateSetting", data);

	ret = snprintf(data,sizeof(data),"0x%02x",intf->desc->bNumEndpoints);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(intf->dev, "bNumEndpoints", data);

	ret = snprintf(data,sizeof(data),"0x%02x",intf->desc->bInterfaceClass);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(intf->dev, "bInterfaceClass", data);

	ret = snprintf(data,sizeof(data),"0x%02x",intf->desc->bInterfaceSubClass);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(intf->dev, "bInterfaceSubClass", data);

	ret = snprintf(data,sizeof(data),"0x%02x",intf->desc->bInterfaceProtocol);
	if (ret < 0) {
		panic("add_device_attributes: snprintf failed");
	}
	devman_usb_add_attr(intf->dev, "bInterfaceProtocol", data);
	
	devman_usb_add_attr(intf->dev, "dev_type", "USB_INTF");
}


/****************************************************************************
 *      devman_usb_device_new                                               *
 ***************************************************************************/
struct devman_usb_dev* 
devman_usb_device_new
(int dev_id)
{
	struct devman_usb_dev *udev = NULL;
	struct devman_dev * dev = NULL;
	
	udev = (struct devman_usb_dev *) malloc(sizeof(struct devman_usb_dev));

	CHECKOUTOFMEM(udev);

	/* allocate device */
	dev = (struct devman_dev *) malloc(sizeof(struct devman_dev));

	CHECKOUTOFMEM(dev);
	
	udev->dev_id = dev_id;
	udev->dev    = dev;
			
	dev->parent_dev_id = 0; /* For now add it directly to the root dev */

	snprintf(dev->name, DEVMAN_DEV_NAME_LEN, "USB%d", dev_id);
	
	TAILQ_INIT(&dev->attrs);
	
	return udev;
}

/****************************************************************************
 *     devman_usb_device_delete                                             *
 ***************************************************************************/
void devman_usb_device_delete(struct devman_usb_dev *udev)
{
	int i;
	struct devman_static_attribute *attr,*temp;
	
	
	for (i=0; i < udev->intf_count; i++) {
		TAILQ_FOREACH_SAFE(attr, &udev->interfaces[i].dev->attrs, list, temp)
		{
			free(attr->name);
			free(attr->data);
			free(attr);
		}
		free(udev->interfaces[i].dev); 
	}

	TAILQ_FOREACH_SAFE(attr, &udev->dev->attrs, list, temp) {
		free(attr->name);
		free(attr->data);
		free(attr);
	}

	free(udev->dev);
	free(udev);
}

static int devman_usb_bind_cb(void *data, endpoint_t ep) {
	if (bind_cb) {
		return bind_cb((struct devman_usb_bind_cb_data *) data, ep);
	} else {
		return ENODEV;
	}
}

static int devman_usb_unbind_cb(void *data, endpoint_t ep) {
	if (unbind_cb) {
		return unbind_cb((struct devman_usb_bind_cb_data *) data, ep);
	} else {
		return ENODEV;
	}
}

/****************************************************************************
 *     devman_usb_device_add                                                *
 ***************************************************************************/
int devman_usb_device_add(struct devman_usb_dev *dev)
{
	int i,res = 0;
	add_device_attributes(dev);

	/* add the USB device */
	dev->cb_data.dev_id    = dev->dev_id; 
	dev->cb_data.interface = -1;
	
	dev->dev->bind_cb   = devman_usb_bind_cb;
	dev->dev->unbind_cb = devman_usb_unbind_cb;
	dev->dev->data       = &dev->cb_data;

	res = devman_add_device(dev->dev);

	if (res != 0) {
		panic("devman_usb_device_add(): devman_add_device failed.");
	}

	/* add the USB interfaces */
	for (i=0; i < dev->intf_count; i++) {
		/* prepare */
		dev->interfaces[i].dev = 
		    (struct devman_dev *) malloc(sizeof(struct devman_dev));
		CHECKOUTOFMEM(dev->interfaces[i].dev);
		
		TAILQ_INIT(&dev->interfaces[i].dev->attrs);
		snprintf(dev->interfaces[i].dev->name, DEVMAN_DEV_NAME_LEN, 
		    "intf%d", i);

		add_interface_attributes(&dev->interfaces[i]);
		
		dev->interfaces[i].dev->parent_dev_id = dev->dev->dev_id;
		
		
		dev->interfaces[i].cb_data.dev_id    = dev->dev_id; 
		dev->interfaces[i].cb_data.interface = 
		    dev->interfaces[i].desc->bInterfaceNumber;

		dev->interfaces[i].dev->bind_cb   = devman_usb_bind_cb;
		dev->interfaces[i].dev->unbind_cb = devman_usb_unbind_cb;
		dev->interfaces[i].dev->data      = &dev->interfaces[i].cb_data;

		/* add */
		res = devman_add_device(dev->interfaces[i].dev);
		
		if (res != 0) {
			panic("devman_usb_device_add(): devman_add_device failed.");
		}
	}

	return res;
}

/****************************************************************************
 *     devman_usb_device_remove                                             *
 ***************************************************************************/
int devman_usb_device_remove(struct devman_usb_dev *dev)
{
	int i, res = 0;

	for (i=0; i < dev->intf_count; i++) {
		
		res = devman_del_device(dev->interfaces[i].dev);

		if (res != 0) {
			panic("devman_usb_device_remove(): devman_del_device failed.");
		}
	}
	res = devman_del_device(dev->dev);
	return res;
}

/****************************************************************************
 *     devman_usb_init                                                      *
 ***************************************************************************/
void devman_usb_init
(int (*_bind_cb)   (struct devman_usb_bind_cb_data *data, endpoint_t ep),
 int (*_unbind_cb) (struct devman_usb_bind_cb_data *data, endpoint_t ep))
{
	bind_cb   = _bind_cb;
	unbind_cb = _unbind_cb;
}
