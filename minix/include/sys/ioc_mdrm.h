/*	sys/ioc_mdrm.h - "DRM-lite" display device ioctls (/dev/drm0).
 *
 * A deliberately small subset of the Linux DRM/KMS model, enough for a
 * userland libdrm backend to offer one CRTC + one connector + one primary
 * plane with dumb (CPU-rendered) buffers:
 *
 *   - Every open() of /dev/drm0 is a separate client (the driver clones the
 *     minor), owning the buffers it creates; they are freed on close().
 *   - Dumb buffers live in video memory.  MDRM_MAP_DUMB returns an offset
 *     to pass to mmap(2) on the same fd (MAP_SHARED, page aligned).
 *   - The first client to set a mode becomes the "master"; other clients
 *     get EBUSY for MDRM_SETCRTC/MDRM_PAGE_FLIP until it closes the device,
 *     which turns the display off again (back to the text console).
 *   - MDRM_PAGE_FLIP takes effect at the next (emulated, 60 Hz) vblank.  With
 *     MDRM_FLIP_EVENT the completion is reported by read(2) on the fd as a
 *     struct mdrm_event_vblank, which has the layout of Linux's
 *     struct drm_event_vblank (type DRM_EVENT_FLIP_COMPLETE); select(2) and
 *     poll(2) report the fd readable while events are pending.
 *
 * The struct layouts of the dumb buffer calls match Linux's drm_mode_*_dumb.
 *
 * Buffer sharing ("PRIME", Linux dma-buf), version 2:
 *   - Buffers are reference counted.  A client refers to a buffer by a
 *     handle; MDRM_DESTROY_DUMB drops the client's handle (= Linux GEM_CLOSE)
 *     and the memory goes when no handle, dma-buf fd or scanout uses it.
 *   - MDRM_PRIME_EXPORT gives a client's buffer a token.  MDRM_PRIME_BIND on
 *     a freshly opened /dev/drm0 turns that open into the buffer's dma-buf
 *     fd: it keeps the buffer alive, mmap(2) at offset 0 maps the buffer, and
 *     MDRM_PRIME_INFO on it returns the token and size.  It takes no other
 *     calls.
 *   - MDRM_PRIME_IMPORT gives a client a handle for a token; a client that
 *     already has a handle for that buffer gets the same one back, as with
 *     Linux.  Tokens are not secrets: like the device, they are for
 *     root/operator only.
 *   - MDRM_DUP_HANDLE gives a client a second handle for one of its
 *     buffers (handle in, new handle out).  libdrm's framebuffers hold one,
 *     so that a framebuffer outlives the handle it was created from (Linux
 *     KMS framebuffers keep their buffer; wlroots closes the handle right
 *     after ADDFB2).
 */

#ifndef _SYS_IOC_MDRM_H
#define _SYS_IOC_MDRM_H

#include <minix/ioctl.h>
#include <stdint.h>

#define MDRM_VERSION		2	/* 2: PRIME */

#define MDRM_MAX_MODES		16

struct mdrm_mode {
	uint16_t	width;
	uint16_t	height;
	uint16_t	refresh;	/* Hz */
	uint16_t	flags;		/* MDRM_MODE_* */
};
#define MDRM_MODE_PREFERRED	0x1

struct mdrm_info {
	char		name[16];	/* driver name, e.g. "bochs" */
	uint32_t	version;	/* MDRM_VERSION */
	uint32_t	vram_size;	/* bytes of video memory */
	uint16_t	max_width;
	uint16_t	max_height;
	uint32_t	nmodes;
	struct mdrm_mode modes[MDRM_MAX_MODES];
	/* Current state. */
	uint16_t	cur_width;	/* 0: display off (text console) */
	uint16_t	cur_height;
	uint32_t	cur_fb;		/* buffer being scanned out, 0: none */
	uint32_t	vblank_seq;	/* number of emulated vblanks so far */
	uint32_t	is_master;	/* this client is the master */
};

struct mdrm_create_dumb {	/* = struct drm_mode_create_dumb */
	uint32_t	height;		/* in */
	uint32_t	width;		/* in */
	uint32_t	bpp;		/* in: only 32 (XRGB8888) */
	uint32_t	flags;		/* in: must be 0 */
	uint32_t	handle;		/* out */
	uint32_t	pitch;		/* out: bytes per line */
	uint64_t	size;		/* out: bytes */
};

struct mdrm_map_dumb {		/* = struct drm_mode_map_dumb */
	uint32_t	handle;		/* in */
	uint32_t	pad;
	uint64_t	offset;		/* out: mmap() offset */
};

struct mdrm_destroy_dumb {	/* = struct drm_mode_destroy_dumb */
	uint32_t	handle;
};

/* Set the mode and scan out a buffer, or turn the display off (fb == 0).
 * The buffer must be width x height (or larger) with the pitch MDRM_CREATE_DUMB
 * returned for that width.
 */
struct mdrm_setcrtc {
	uint32_t	fb;		/* buffer handle, 0: display off */
	uint16_t	width;
	uint16_t	height;
};

/* Scan out another buffer of the current mode at the next vblank. */
struct mdrm_page_flip {
	uint32_t	fb;		/* buffer handle */
	uint32_t	flags;		/* MDRM_FLIP_* */
	uint64_t	user_data;	/* echoed in the completion event */
	uint32_t	crtc_id;	/* echoed in the completion event */
	uint32_t	pad;
};
#define MDRM_FLIP_EVENT		0x01	/* = DRM_MODE_PAGE_FLIP_EVENT */

struct mdrm_event_vblank {	/* = struct drm_event_vblank */
	uint32_t	type;		/* MDRM_EVENT_FLIP_COMPLETE */
	uint32_t	length;		/* sizeof(struct mdrm_event_vblank) */
	uint64_t	user_data;
	uint32_t	tv_sec;		/* completion time, CLOCK_MONOTONIC */
	uint32_t	tv_usec;
	uint32_t	sequence;
	uint32_t	crtc_id;
};
#define MDRM_EVENT_FLIP_COMPLETE 0x02	/* = DRM_EVENT_FLIP_COMPLETE */

struct mdrm_prime {
	uint32_t	handle;		/* EXPORT: in; IMPORT: out */
	uint32_t	token;		/* EXPORT, INFO: out; BIND, IMPORT: in */
	uint64_t	size;		/* BIND, INFO, IMPORT: out (bytes) */
};

#define MDRM_GETINFO		_IOR('g', 1, struct mdrm_info)
#define MDRM_CREATE_DUMB	_IOWR('g', 2, struct mdrm_create_dumb)
#define MDRM_MAP_DUMB		_IOWR('g', 3, struct mdrm_map_dumb)
#define MDRM_DESTROY_DUMB	_IOW('g', 4, struct mdrm_destroy_dumb)
#define MDRM_SETCRTC		_IOW('g', 5, struct mdrm_setcrtc)
#define MDRM_PAGE_FLIP		_IOW('g', 6, struct mdrm_page_flip)
#define MDRM_PRIME_EXPORT	_IOWR('g', 7, struct mdrm_prime)
#define MDRM_PRIME_BIND		_IOWR('g', 8, struct mdrm_prime)
#define MDRM_PRIME_INFO		_IOWR('g', 9, struct mdrm_prime)
#define MDRM_PRIME_IMPORT	_IOWR('g', 10, struct mdrm_prime)
#define MDRM_DUP_HANDLE		_IOWR('g', 11, struct mdrm_prime)

#endif /* _SYS_IOC_MDRM_H */
