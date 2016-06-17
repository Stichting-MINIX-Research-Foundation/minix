/*
 * Generic virtio library for MINIX 3
 *
 * Copyright (c) 2013, A. Welzel, <arne.welzel@gmail.com>
 *
 * This software is released under the BSD license. See the LICENSE file
 * included in the main directory of this source distribution for the
 * license terms and conditions.
 */

#define _SYSTEM 1

#include <assert.h>
#include <errno.h>				/* for OK... */
#include <string.h>				/* memset() */
#include <stdlib.h>				/* malloc() */

#include <machine/pci.h>			/* PCI_ILR, PCI_BAR... */
#include <machine/vmparam.h>			/* PAGE_SIZE */

#include <minix/syslib.h>			/* umap, vumap, alloc_..*/
#include <minix/sysutil.h>			/* panic(), at least */
#include <minix/virtio.h>			/* virtio system include */

#include "virtio_ring.h"			/* virtio types / helper */

/*
 * About indirect descriptors:
 *
 * For each possible thread, a single indirect descriptor table is allocated.
 * If using direct descriptors would lead to the situation that another thread
 * might not be able to add another descriptor to the ring, indirect descriptors
 * are used.
 *
 * Indirect descriptors are pre-allocated. Each alloc_contig() call involves a
 * kernel call which is critical for performance.
 *
 * The size of indirect descriptor tables is chosen based on MAPVEC_NR. A driver
 * using this library should never add more than
 *
 *    MAPVEC_NR + MAPVEC_NR / 2
 *
 * descriptors to a queue as this represent the maximum size of an indirect
 * descriptor table.
 */

struct indirect_desc_table {
	int in_use;
	struct vring_desc *descs;
	phys_bytes paddr;
	size_t len;
};

struct virtio_queue {

	void *vaddr;				/* virtual addr of ring */
	phys_bytes paddr;			/* physical addr of ring */
	u32_t page;				/* physical guest page */

	u16_t num;				/* number of descriptors */
	u32_t ring_size;			/* size of ring in bytes */
	struct vring vring;

	u16_t free_num;				/* free descriptors */
	u16_t free_head;			/* next free descriptor */
	u16_t free_tail;			/* last free descriptor */
	u16_t last_used;			/* we checked in used */

	void **data;				/* points to pointers */
};

struct virtio_device {

	const char *name;			/* for debugging */

	u16_t  port;				/* io port */

	struct virtio_feature *features;	/* host / guest features */
	u8_t num_features;			/* max 32 */

	struct virtio_queue *queues;		/* our queues */
	u16_t num_queues;

	int irq;				/* interrupt line */
	int irq_hook;				/* hook id */
	int msi;				/* is MSI enabled? */

	int threads;				/* max number of threads */

	struct indirect_desc_table *indirect;	/* indirect descriptor tables */
	int num_indirect;
};

static int is_matching_device(u16_t expected_sdid, u16_t vid, u16_t sdid);
static int init_device(int devind, struct virtio_device *dev);
static int init_phys_queues(struct virtio_device *dev);
static int exchange_features(struct virtio_device *dev);
static int alloc_phys_queue(struct virtio_queue *q);
static void free_phys_queue(struct virtio_queue *q);
static void init_phys_queue(struct virtio_queue *q);
static int init_indirect_desc_table(struct indirect_desc_table *desc);
static int init_indirect_desc_tables(struct virtio_device *dev);
static void virtio_irq_register(struct virtio_device *dev);
static void virtio_irq_unregister(struct virtio_device *dev);
static int wants_kick(struct virtio_queue *q);
static void kick_queue(struct virtio_device *dev, int qidx);

struct virtio_device *
virtio_setup_device(u16_t subdevid, const char *name,
		struct virtio_feature *features, int num_features,
		int threads, int skip)
{
	int r, devind;
	u16_t vid, did, sdid;
	struct virtio_device *ret;

	/* bogus values? */
	if (skip < 0 || name == NULL || num_features < 0 || threads <= 0)
		return NULL;

	pci_init();

	r = pci_first_dev(&devind, &vid, &did);

	while (r > 0) {
		sdid = pci_attr_r16(devind, PCI_SUBDID);
		if (is_matching_device(subdevid, vid, sdid)) {

			/* this is the device we are looking for */
			if (skip == 0)
				break;

			skip--;
		}

		r = pci_next_dev(&devind, &vid, &did);
	}

	/* pci_[first|next_dev()] return 0 if no device was found */
	if (r == 0 || skip > 0)
		return NULL;

	/* allocate and set known info about the device */
	ret = malloc(sizeof(*ret));

	if (ret == NULL)
		return NULL;

	/* Prepare virtio_device intance */
	memset(ret, 0, sizeof(*ret));
	ret->name = name;
	ret->features = features;
	ret->num_features = num_features;
	ret->threads = threads;
	/* see comment in the beginning of this file */
	ret->num_indirect = threads;

	if (init_device(devind, ret) != OK) {
		printf("%s: Could not initialize device\n", ret->name);
		goto err;
	}

	/* Ack the device */
	virtio_write8(ret, VIRTIO_DEV_STATUS_OFF, VIRTIO_STATUS_ACK);

	if (exchange_features(ret) != OK) {
		printf("%s: Could not exchange features\n", ret->name);
		goto err;
	}

	if (init_indirect_desc_tables(ret) != OK) {
		printf("%s: Could not initialize indirect tables\n", ret->name);
		goto err;
	}

	/* We know how to drive the device... */
	virtio_write8(ret, VIRTIO_DEV_STATUS_OFF, VIRTIO_STATUS_DRV);

	return ret;

/* Error path */
err:
	free(ret);
	return NULL;
}

static int
init_device(int devind, struct virtio_device *dev)
{
	u32_t base, size;
	int iof, r;

	pci_reserve(devind);

	if ((r = pci_get_bar(devind, PCI_BAR, &base, &size, &iof)) != OK) {
		printf("%s: Could not get BAR (%d)", dev->name, r);
		return r;
	}

	if (!iof) {
		printf("%s: PCI not IO space?", dev->name);
		return EINVAL;
	}

	if (base & 0xFFFF0000) {
		printf("%s: IO port weird (%08x)", dev->name, base);
		return EINVAL;
	}

	/* store the I/O port */
	dev->port = base;

	/* Reset the device */
	virtio_write8(dev, VIRTIO_DEV_STATUS_OFF, 0);

	/* Read IRQ line */
	dev->irq = pci_attr_r8(devind, PCI_ILR);

	return OK;
}

static int
exchange_features(struct virtio_device *dev)
{
	u32_t guest_features = 0, host_features = 0;
	struct virtio_feature *f;

	host_features = virtio_read32(dev, VIRTIO_HOST_F_OFF);

	for (int i = 0; i < dev->num_features; i++) {
		f = &dev->features[i];

		/* prepare the features the driver supports */
		guest_features |= (f->guest_support << f->bit);

		/* just load the host feature int the struct */
		f->host_support =  ((host_features >> f->bit) & 1);
	}

	/* let the device know about our features */
	virtio_write32(dev, VIRTIO_GUEST_F_OFF, guest_features);

	return OK;
}

int
virtio_alloc_queues(struct virtio_device *dev, int num_queues)
{
	int r = OK;

	assert(dev != NULL);

	/* Assume there's no device with more than 256 queues */
	if (num_queues < 0 || num_queues > 256)
		return EINVAL;

	dev->num_queues = num_queues;
	/* allocate queue memory */
	dev->queues = malloc(num_queues * sizeof(dev->queues[0]));

	if (dev->queues == NULL)
		return ENOMEM;

	memset(dev->queues, 0, num_queues * sizeof(dev->queues[0]));

	if ((r = init_phys_queues(dev)) != OK) {
		printf("%s: Could not initialize queues (%d)\n", dev->name, r);
		free(dev->queues);
		dev->queues = NULL;
	}

	return r;
}

static int
init_phys_queues(struct virtio_device *dev)
{
	/* Initialize all queues */
	int i, j, r;
	struct virtio_queue *q;

	for (i = 0; i < dev->num_queues; i++) {
		q = &dev->queues[i];
		/* select the queue */
		virtio_write16(dev, VIRTIO_QSEL_OFF, i);
		q->num = virtio_read16(dev, VIRTIO_QSIZE_OFF);

		if (q->num & (q->num - 1)) {
			printf("%s: Queue %d num=%d not ^2", dev->name, i,
							     q->num);
			r = EINVAL;
			goto free_phys_queues;
		}

		if ((r = alloc_phys_queue(q)) != OK)
			goto free_phys_queues;

		init_phys_queue(q);

		/* Let the host know about the guest physical page */
		virtio_write32(dev, VIRTIO_QADDR_OFF, q->page);
	}

	return OK;

/* Error path */
free_phys_queues:
	for (j = 0; j < i; j++)
		free_phys_queue(&dev->queues[i]);

	return r;
}

static int
alloc_phys_queue(struct virtio_queue *q)
{
	assert(q != NULL);

	/* How much memory do we need? */
	q->ring_size = vring_size(q->num, PAGE_SIZE);

	q->vaddr = alloc_contig(q->ring_size, AC_ALIGN4K, &q->paddr);

	if (q->vaddr == NULL)
		return ENOMEM;

	q->data = alloc_contig(sizeof(q->data[0]) * q->num, AC_ALIGN4K, NULL);

	if (q->data == NULL) {
		free_contig(q->vaddr, q->ring_size);
		q->vaddr = NULL;
		q->paddr = 0;
		return ENOMEM;
	}

	return OK;
}

void
virtio_device_ready(struct virtio_device *dev)
{
	assert(dev != NULL);

	/* Register IRQ line */
	virtio_irq_register(dev);

	/* Driver is ready to go! */
	virtio_write8(dev, VIRTIO_DEV_STATUS_OFF, VIRTIO_STATUS_DRV_OK);
}

void
virtio_free_queues(struct virtio_device *dev)
{
	int i;
	assert(dev != NULL);
	assert(dev->queues != NULL);
	assert(dev->num_queues > 0);

	for (i = 0; i < dev->num_queues; i++)
		free_phys_queue(&dev->queues[i]);

	dev->num_queues = 0;
	dev->queues = NULL;
}

static void
free_phys_queue(struct virtio_queue *q)
{
	assert(q != NULL);
	assert(q->vaddr != NULL);

	free_contig(q->vaddr, q->ring_size);
	q->vaddr = NULL;
	q->paddr = 0;
	q->num = 0;
	free_contig(q->data, sizeof(q->data[0]));
	q->data = NULL;
}

static void
init_phys_queue(struct virtio_queue *q)
{
	memset(q->vaddr, 0, q->ring_size);
	memset(q->data, 0, sizeof(q->data[0]) * q->num);

	/* physical page in guest */
	q->page = q->paddr / PAGE_SIZE;

	/* Set pointers in q->vring according to size */
	vring_init(&q->vring, q->num, q->vaddr, PAGE_SIZE);

	/* Everything's free at this point */
	for (int i = 0; i < q->num; i++) {
		q->vring.desc[i].flags = VRING_DESC_F_NEXT;
		q->vring.desc[i].next = (i + 1) & (q->num - 1);
	}

	q->free_num = q->num;
	q->free_head = 0;
	q->free_tail = q->num - 1;
	q->last_used = 0;

	return;
}

void
virtio_free_device(struct virtio_device *dev)
{
	int i;
	struct indirect_desc_table *desc;

	assert(dev != NULL);

	assert(dev->num_indirect > 0);

	for (i = 0; i < dev->num_indirect; i++) {
		desc = &dev->indirect[i];
		free_contig(desc->descs, desc->len);
	}

	dev->num_indirect = 0;

	assert(dev->indirect != NULL);
	free(dev->indirect);
	dev->indirect = NULL;

	free(dev);
}

static int
init_indirect_desc_table(struct indirect_desc_table *desc)
{
	desc->in_use = 0;
	desc->len = (MAPVEC_NR + MAPVEC_NR / 2) * sizeof(struct vring_desc);

	desc->descs = alloc_contig(desc->len, AC_ALIGN4K, &desc->paddr);
	memset(desc->descs, 0, desc->len);

	if (desc->descs == NULL)
		return ENOMEM;

	return OK;
}

static int
init_indirect_desc_tables(struct virtio_device *dev)
{
	int i, j, r;
	struct indirect_desc_table *desc;

	dev->indirect = malloc(dev->num_indirect * sizeof(dev->indirect[0]));

	if (dev->indirect == NULL) {
		printf("%s: Could not allocate indirect tables\n", dev->name);
		return ENOMEM;
	}

	memset(dev->indirect, 0, dev->num_indirect* sizeof(dev->indirect[0]));

	for (i = 0; i < dev->num_indirect; i++) {
		desc = &dev->indirect[i];
		if ((r = init_indirect_desc_table(desc)) != OK) {

			/* error path */
			for (j = 0; j < i; j++) {
				desc = &dev->indirect[j];
				free_contig(desc->descs, desc->len);
			}

			free(dev->indirect);

			return r;
		}
	}

	return OK;
}

static void
clear_indirect_table(struct virtio_device *dev, struct vring_desc *vd)
{
	int i;
	struct indirect_desc_table *desc;

	assert(vd->len > 0);
	assert(vd->flags & VRING_DESC_F_INDIRECT);
	vd->flags = vd->flags & ~VRING_DESC_F_INDIRECT;
	vd->len = 0;;

	for (i = 0; i < dev->num_indirect; i++) {
		desc = &dev->indirect[i];

		if (desc->paddr == vd->addr) {
			assert(desc->in_use);
			desc->in_use = 0;
			break;
		}
	}

	if (i >= dev->num_indirect)
		panic("Could not clear indirect descriptor table ");
}


inline static void
use_vring_desc(struct vring_desc *vd, struct vumap_phys *vp)
{
	vd->addr = vp->vp_addr & ~1UL;
	vd->len = vp->vp_size;
	vd->flags = VRING_DESC_F_NEXT;

	if (vp->vp_addr & 1)
		vd->flags |= VRING_DESC_F_WRITE;
}

static void
set_indirect_descriptors(struct virtio_device *dev, struct virtio_queue *q,
	struct vumap_phys *bufs, size_t num)
{
	/* Indirect descriptor tables are simply filled from left to right */
	int i;
	struct indirect_desc_table *desc;
	struct vring *vring = &q->vring;
	struct vring_desc *vd, *ivd = NULL;

	if (0 == num)
		return;

	/* Find the first unused indirect descriptor table */
	for (i = 0; i < dev->num_indirect; i++) {
		desc = &dev->indirect[i];

		/* If an unused indirect descriptor table was found,
		 * mark it as being used and exit the loop.
		 */
		if (!desc->in_use) {
			desc->in_use = 1;
			break;
		}
	}

	/* Sanity check */
	if (i >= dev->num_indirect)
		panic("No indirect descriptor tables left");

	/* For indirect descriptor tables, only a single descriptor from
	 * the main ring is used.
	 */
	vd = &vring->desc[q->free_head];
	vd->flags = VRING_DESC_F_INDIRECT;
	vd->addr = desc->paddr;
	vd->len = num * sizeof(desc->descs[0]);

	/* Initialize the descriptors in the indirect descriptor table */
	for (i = 0; i < (int)num; i++) {
		ivd = &desc->descs[i];

		use_vring_desc(ivd, &bufs[i]);
		ivd->next = i + 1;
	}

	/* Unset the next bit of the last descriptor */
	if (NULL != ivd)
		ivd->flags = ivd->flags & ~VRING_DESC_F_NEXT;

	/* Update queue, only a single descriptor was used */
	q->free_num -= 1;
	q->free_head = vd->next;
}

static void
set_direct_descriptors(struct virtio_queue *q, struct vumap_phys *bufs,
	size_t num)
{
	u16_t i;
	size_t count;
	struct vring *vring = &q->vring;
	struct vring_desc *vd;

	if (0 == num)
		return;

	for (i = q->free_head, count = 0; count < num; count++) {

		/* The next free descriptor */
		vd = &vring->desc[i];

		/* The descriptor is linked in the free list, so
		 * it always has the next bit set.
		 */
		assert(vd->flags & VRING_DESC_F_NEXT);

		use_vring_desc(vd, &bufs[count]);
		i = vd->next;
	}

	/* Unset the next bit of the last descriptor */
	vd->flags = vd->flags & ~VRING_DESC_F_NEXT;

	/* Update queue */
	q->free_num -= num;
	q->free_head = i;
}

int
virtio_to_queue(struct virtio_device *dev, int qidx, struct vumap_phys *bufs,
	size_t num, void *data)
{
	u16_t free_first;
	int left;
	struct virtio_queue *q = &dev->queues[qidx];
	struct vring *vring = &q->vring;

	assert(0 <= qidx && qidx <= dev->num_queues);

	if (!data)
		panic("%s: NULL data received queue %d", dev->name, qidx);

	free_first = q->free_head;

	left = (int)q->free_num - (int)num;

	if (left < dev->threads)
		set_indirect_descriptors(dev, q, bufs, num);
	else
		set_direct_descriptors(q, bufs, num);

	/* Next index for host is old free_head */
	vring->avail->ring[vring->avail->idx % q->num] = free_first;

	/* Provided by the caller to identify this slot */
	q->data[free_first] = data;

	/* Make sure the host sees the new descriptors */
	__insn_barrier();

	/* advance last idx */
	vring->avail->idx += 1;

	/* Make sure the host sees the avail->idx */
	__insn_barrier();

	/* kick it! */
	kick_queue(dev, qidx);
	return 0;
}

int
virtio_from_queue(struct virtio_device *dev, int qidx, void **data,
	size_t *len)
{
	struct virtio_queue *q;
	struct vring *vring;
	struct vring_used_elem *uel;
	struct vring_desc *vd;
	int count = 0;
	u16_t idx;
	u16_t used_idx;

	assert(0 <= qidx && qidx < dev->num_queues);

	q = &dev->queues[qidx];
	vring = &q->vring;

	/* Make sure we see changes done by the host */
	__insn_barrier();

	/* The index from the host */
	used_idx = vring->used->idx % q->num;

	/* We already saw this one, nothing to do here */
	if (q->last_used == used_idx)
		return -1;

	/* Get the vring_used element */
	uel = &q->vring.used->ring[q->last_used];

	/* Update the last used element */
	q->last_used = (q->last_used + 1) % q->num;

	/* index of the used element */
	idx = uel->id % q->num;

	assert(q->data[idx] != NULL);

	/* Get the descriptor */
	vd = &vring->desc[idx];

	/* Unconditionally set the tail->next to the first used one */
	assert(vring->desc[q->free_tail].flags & VRING_DESC_F_NEXT);
	vring->desc[q->free_tail].next = idx;

	/* Find the last index, eventually there has to be one
	 * without a the next flag.
	 *
	 * FIXME: Protect from endless loop
	 */
	while (vd->flags & VRING_DESC_F_NEXT) {

		if (vd->flags & VRING_DESC_F_INDIRECT)
			clear_indirect_table(dev, vd);

		idx = vd->next;
		vd = &vring->desc[idx];
		count++;
	}

	/* Didn't count the last one */
	count++;

	if (vd->flags & VRING_DESC_F_INDIRECT)
		clear_indirect_table(dev, vd);

	/* idx points to the tail now, update the queue */
	q->free_tail = idx;
	assert(!(vd->flags & VRING_DESC_F_NEXT));

	/* We can always connect the tail with the head */
	vring->desc[q->free_tail].next = q->free_head;
	vring->desc[q->free_tail].flags = VRING_DESC_F_NEXT;

	q->free_num += count;

	assert(q->free_num <= q->num);

	*data = q->data[uel->id];
	q->data[uel->id] = NULL;

	if (len != NULL)
		*len = uel->len;

	return 0;
}

int
virtio_had_irq(struct virtio_device *dev)
{
	return virtio_read8(dev, VIRTIO_ISR_STATUS_OFF) & 1;
}

void
virtio_reset_device(struct virtio_device *dev)
{
	virtio_irq_unregister(dev);
	virtio_write8(dev, VIRTIO_DEV_STATUS_OFF, 0);
}


void
virtio_irq_enable(struct virtio_device *dev)
{
	int r;
	if ((r = sys_irqenable(&dev->irq_hook)) != OK)
		panic("%s Unable to enable IRQ %d", dev->name, r);
}

void
virtio_irq_disable(struct virtio_device *dev)
{
	int r;
	if ((r = sys_irqdisable(&dev->irq_hook)) != OK)
		panic("%s: Unable to disable IRQ %d", dev->name, r);
}

static int
wants_kick(struct virtio_queue *q)
{
	assert(q != NULL);
	return !(q->vring.used->flags & VRING_USED_F_NO_NOTIFY);
}

static void
kick_queue(struct virtio_device *dev, int qidx)
{
	assert(0 <= qidx && qidx < dev->num_queues);

	if (wants_kick(&dev->queues[qidx]))
		virtio_write16(dev, VIRTIO_QNOTFIY_OFF, qidx);

	return;
}

static int
is_matching_device(u16_t expected_sdid, u16_t vid, u16_t sdid)
{
	return vid == VIRTIO_VENDOR_ID && sdid == expected_sdid;
}

static void
virtio_irq_register(struct virtio_device *dev)
{
	int r;
	if ((r = sys_irqsetpolicy(dev->irq, 0, &dev->irq_hook)) != OK)
		panic("%s: Unable to register IRQ %d", dev->name, r);
}

static void
virtio_irq_unregister(struct virtio_device *dev)
{
	int r;
	if ((r = sys_irqrmpolicy(&dev->irq_hook)) != OK)
		panic("%s: Unable to unregister IRQ %d", dev->name, r);
}

static int
_supports(struct virtio_device *dev, int bit, int host)
{
	for (int i = 0; i < dev->num_features; i++) {
		struct virtio_feature *f = &dev->features[i];

		if (f->bit == bit)
			return host ? f->host_support : f->guest_support;
	}

	panic("%s: Feature not found bit=%d", dev->name, bit);
}

int
virtio_host_supports(struct virtio_device *dev, int bit)
{
	return _supports(dev, bit, 1);
}

int
virtio_guest_supports(struct virtio_device *dev, int bit)
{
	return _supports(dev, bit, 0);
}


/* Just some wrappers around sys_read */
#define VIRTIO_READ_XX(xx, suff)					\
u##xx##_t								\
virtio_read##xx(struct virtio_device *dev, i32_t off)			\
{									\
	int r;								\
	u32_t ret;							\
	if ((r = sys_in##suff(dev->port + off, &ret)) != OK)		\
		panic("%s: Read failed %d %d r=%d", dev->name,		\
						    dev->port,		\
						    off,		\
						    r);			\
									\
	return ret;							\
}

VIRTIO_READ_XX(32, l)
VIRTIO_READ_XX(16, w)
VIRTIO_READ_XX(8, b)

/* Just some wrappers around sys_write */
#define VIRTIO_WRITE_XX(xx, suff)					\
void									\
virtio_write##xx(struct virtio_device *dev, i32_t off, u##xx##_t val)	\
{									\
	int r;								\
	if ((r = sys_out##suff(dev->port + off, val)) != OK)		\
		panic("%s: Write failed %d %d r=%d", dev->name,		\
						     dev->port,		\
						     off,		\
						     r);		\
}

VIRTIO_WRITE_XX(32, l)
VIRTIO_WRITE_XX(16, w)
VIRTIO_WRITE_XX(8, b)

/* Just some wrappers around sys_read */
#define VIRTIO_SREAD_XX(xx, suff)					\
u##xx##_t								\
virtio_sread##xx(struct virtio_device *dev, i32_t off)			\
{									\
	int r;								\
	u32_t ret;							\
	off += VIRTIO_DEV_SPECIFIC_OFF; 				\
									\
	if (dev->msi)							\
		off += VIRTIO_MSI_ADD_OFF;				\
									\
	if ((r = sys_in##suff(dev->port + off, &ret)) != OK)		\
		panic("%s: Read failed %d %d r=%d", dev->name,		\
						    dev->port,		\
						    off,		\
						    r);			\
									\
	return ret;							\
}

VIRTIO_SREAD_XX(32, l)
VIRTIO_SREAD_XX(16, w)
VIRTIO_SREAD_XX(8, b)

/* Just some wrappers around sys_write */
#define VIRTIO_SWRITE_XX(xx, suff)					\
void									\
virtio_swrite##xx(struct virtio_device *dev, i32_t off, u##xx##_t val)	\
{									\
	int r;								\
	off += VIRTIO_DEV_SPECIFIC_OFF; 				\
									\
	if (dev->msi)							\
		off += VIRTIO_MSI_ADD_OFF;				\
									\
	if ((r = sys_out##suff(dev->port + off, val)) != OK)		\
		panic("%s: Write failed %d %d r=%d", dev->name,		\
						     dev->port,		\
						     off,		\
						     r);		\
}

VIRTIO_SWRITE_XX(32, l)
VIRTIO_SWRITE_XX(16, w)
VIRTIO_SWRITE_XX(8, b)
