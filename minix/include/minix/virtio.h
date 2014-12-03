/*
 * Generic virtio library for MINIX 3
 *
 * Copyright (c) 2013, A. Welzel, <arne.welzel@gmail.com>
 *
 * This software is released under the BSD license. See the LICENSE file
 * included in the main directory of this source distribution for the
 * license terms and conditions.
 */

#ifndef _MINIX_VIRTIO_H
#define _MINIX_VIRTIO_H 1

#include <sys/types.h>

#define VIRTIO_VENDOR_ID			0x1AF4

#define VIRTIO_HOST_F_OFF			0x0000
#define VIRTIO_GUEST_F_OFF			0x0004
#define VIRTIO_QADDR_OFF			0x0008

#define VIRTIO_QSIZE_OFF			0x000C
#define VIRTIO_QSEL_OFF				0x000E
#define VIRTIO_QNOTFIY_OFF			0x0010

#define VIRTIO_DEV_STATUS_OFF			0x0012
#define VIRTIO_ISR_STATUS_OFF			0x0013
#define VIRTIO_DEV_SPECIFIC_OFF			0x0014
/* if msi is enabled, device specific headers shift by 4 */
#define VIRTIO_MSI_ADD_OFF			0x0004
#define VIRTIO_STATUS_ACK			0x01
#define VIRTIO_STATUS_DRV			0x02
#define VIRTIO_STATUS_DRV_OK			0x04
#define VIRTIO_STATUS_FAIL			0x80


/* Feature description */
struct virtio_feature {
	const char *name;
	u8_t bit;
	u8_t host_support;
	u8_t guest_support;
};

/* Forward declaration of struct virtio_device.
 *
 * This structure is opaque to the caller.
 */
struct virtio_device;

/* Find a virtio device with subdevice id subdevid. Returns a pointer
 * to an opaque virtio_device instance.
 */
struct virtio_device *virtio_setup_device(u16_t subdevid,
		const char *name,
		struct virtio_feature *features,
		int feature_count,
		int threads, int skip);

/* Attempt to allocate queue_cnt memory for queues */
int virtio_alloc_queues(struct virtio_device *dev, int num_queues);

/* Register the IRQ policy and indicate to the host we are ready to go */
void virtio_device_ready(struct virtio_device *dev);

/* Unregister the IRQ and reset the device */
void virtio_reset_device(struct virtio_device *dev);

/* Free the memory used by all queues */
void virtio_free_queues(struct virtio_device *dev);

/* Free all memory allocated for the device (except the queue memory,
 * which has to be freed before with virtio_free_queues()).
 *
 * Don't touch the device afterwards! This is like free(dev).
 */
void virtio_free_device(struct virtio_device *dev);


/* Feature helpers */
int virtio_guest_supports(struct virtio_device *dev, int bit);
int virtio_host_supports(struct virtio_device *dev, int bit);

/*
 * Use num vumap_phys elements and chain these as vring_desc elements
 * into the vring.
 *
 * Kick the queue if needed.
 *
 * data is opaque and returned by virtio_from_queue() when the host
 * processed the descriptor chain.
 *
 * Note: The last bit of vp_addr is used to flag whether an iovec is
 *	 writable. This implies that only word aligned buffers can be
 *	 used.
 */
int virtio_to_queue(struct virtio_device *dev, int qidx,
			struct vumap_phys *bufs, size_t num, void *data);

/*
 * If the host used a chain of descriptors, return 0, set data as was given to
 * virtio_to_queue(), and if len is not NULL, set it to the resulting length.
 * If the host has not processed any element, return -1.
 */
int virtio_from_queue(struct virtio_device *dev, int qidx, void **data,
	size_t *len);

/* IRQ related functions */
void virtio_irq_enable(struct virtio_device *dev);
void virtio_irq_disable(struct virtio_device *dev);

/* Checks the ISR field of the device and returns true if
 * the interrupt was for this device.
 */
int virtio_had_irq(struct virtio_device *dev);


u32_t virtio_read32(struct virtio_device *dev, i32_t offset);
u16_t virtio_read16(struct virtio_device *dev, i32_t offset);
u8_t virtio_read8(struct virtio_device *dev, i32_t offset);
void virtio_write32(struct virtio_device *dev, i32_t offset, u32_t val);
void virtio_write16(struct virtio_device *dev, i32_t offset, u16_t val);
void virtio_write8(struct virtio_device *dev, i32_t offset, u8_t val);


/*
 * Device specific reads take MSI offset into account and all reads
 * are at offset 20.
 *
 * Something like:
 * read(off) --> readX(20 + (msi ? 4 : 0) + off)
 */
u32_t virtio_sread32(struct virtio_device *dev, i32_t offset);
u16_t virtio_sread16(struct virtio_device *dev, i32_t offset);
u8_t virtio_sread8(struct virtio_device *dev, i32_t offset);
void virtio_swrite32(struct virtio_device *dev, i32_t offset, u32_t val);
void virtio_swrite16(struct virtio_device *dev, i32_t offset, u16_t val);
void virtio_swrite8(struct virtio_device *dev, i32_t offset, u8_t val);

#endif /* _MINIX_VIRTIO_H */
