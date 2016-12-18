/*	$NetBSD: virtioreg.h,v 1.3 2015/05/04 14:08:57 ozaki-r Exp $	*/

/*
 * Copyright (c) 2010 Minoura Makoto.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Part of the file derived from `Virtio PCI Card Specification v0.8.6 DRAFT'
 * Appendix A.
 */
/* An interface for efficient virtio implementation.
 *
 * This header is BSD licensed so anyone can use the definitions
 * to implement compatible drivers/servers.
 *
 * Copyright 2007, 2009, IBM Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifndef _DEV_PCI_VIRTIOREG_H_
#define	_DEV_PCI_VIRTIOREG_H_

#include <sys/types.h>

/* Virtio product id (subsystem) */
#define PCI_PRODUCT_VIRTIO_NETWORK	1
#define PCI_PRODUCT_VIRTIO_BLOCK	2
#define PCI_PRODUCT_VIRTIO_CONSOLE	3
#define PCI_PRODUCT_VIRTIO_ENTROPY	4
#define PCI_PRODUCT_VIRTIO_BALLOON	5
#define PCI_PRODUCT_VIRTIO_9P		9

/* Virtio header */
#define VIRTIO_CONFIG_DEVICE_FEATURES	0 /* 32bit */
#define VIRTIO_CONFIG_GUEST_FEATURES	4 /* 32bit */
#define  VIRTIO_F_NOTIFY_ON_EMPTY		(1<<24)
#define  VIRTIO_F_RING_INDIRECT_DESC		(1<<28)
#define  VIRTIO_F_BAD_FEATURE			(1<<30)
#define VIRTIO_CONFIG_QUEUE_ADDRESS	8 /* 32bit */
#define VIRTIO_CONFIG_QUEUE_SIZE	12 /* 16bit */
#define VIRTIO_CONFIG_QUEUE_SELECT	14 /* 16bit */
#define VIRTIO_CONFIG_QUEUE_NOTIFY	16 /* 16bit */
#define VIRTIO_CONFIG_DEVICE_STATUS	18 /* 8bit */
#define  VIRTIO_CONFIG_DEVICE_STATUS_RESET	0
#define  VIRTIO_CONFIG_DEVICE_STATUS_ACK	1
#define  VIRTIO_CONFIG_DEVICE_STATUS_DRIVER	2
#define  VIRTIO_CONFIG_DEVICE_STATUS_DRIVER_OK	4
#define  VIRTIO_CONFIG_DEVICE_STATUS_FAILED	128
#define VIRTIO_CONFIG_ISR_STATUS	19 /* 8bit */
#define  VIRTIO_CONFIG_ISR_CONFIG_CHANGE	2
#define VIRTIO_CONFIG_CONFIG_VECTOR	20 /* 16bit, optional */
#define VIRTIO_CONFIG_DEVICE_CONFIG_NOMSI	20
#define VIRTIO_CONFIG_DEVICE_CONFIG_MSI		24

/* Virtqueue */
/* This marks a buffer as continuing via the next field. */
#define VRING_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VRING_DESC_F_INDIRECT	4

/* The Host uses this in used->flags to advise the Guest: don't kick me
 * when you add a buffer.  It's unreliable, so it's simply an
 * optimization.  Guest will still kick if it's out of buffers. */
#define VRING_USED_F_NO_NOTIFY  1
/* The Guest uses this in avail->flags to advise the Host: don't
 * interrupt me when you consume a buffer.  It's unreliable, so it's
 * simply an optimization.  */
#define VRING_AVAIL_F_NO_INTERRUPT      1

/* Virtio ring descriptors: 16 bytes.
 * These can chain together via "next". */
struct vring_desc {
        /* Address (guest-physical). */
        uint64_t addr;
        /* Length. */
        uint32_t len;
        /* The flags as indicated above. */
        uint16_t flags;
        /* We chain unused descriptors via this, too */
        uint16_t next;
} __packed;

struct vring_avail {
        uint16_t flags;
        uint16_t idx;
        uint16_t ring[0];
} __packed;

/* u32 is used here for ids for padding reasons. */
struct vring_used_elem {
        /* Index of start of used descriptor chain. */
        uint32_t id;
        /* Total length of the descriptor chain which was written to. */
        uint32_t len;
} __packed;

struct vring_used {
        uint16_t flags;
        uint16_t idx;
        struct vring_used_elem ring[0];
} __packed;

/* The standard layout for the ring is a continuous chunk of memory which
 * looks like this.  We assume num is a power of 2.
 *
 * struct vring {
 *      // The actual descriptors (16 bytes each)
 *      struct vring_desc desc[num];
 *
 *      // A ring of available descriptor heads with free-running index.
 *      __u16 avail_flags;
 *      __u16 avail_idx;
 *      __u16 available[num];
 *
 *      // Padding to the next align boundary.
 *      char pad[];
 *
 *      // A ring of used descriptor heads with free-running index.
 *      __u16 used_flags;
 *      __u16 used_idx;
 *      struct vring_used_elem used[num];
 * };
 * Note: for virtio PCI, align is 4096.
 */

#define VIRTIO_PAGE_SIZE	(4096)

#endif /* _DEV_PCI_VIRTIOREG_H_ */
