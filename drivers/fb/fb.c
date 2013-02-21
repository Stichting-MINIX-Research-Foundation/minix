#include <minix/fb.h>
#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/ds.h>
#include <minix/sysutil.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <sys/ioc_fb.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include "logos.h"
#include "fb.h"

#define FB_DEV_NR	1
/*
 * Function prototypes for the fb driver.
 */
static int fb_open(message *m);
static int fb_close(message *m);
static struct device * fb_prepare(dev_t device);
static int fb_transfer(endpoint_t endpt, int opcode, u64_t position,
	iovec_t *iov, unsigned int nr_req, endpoint_t user_endpt, unsigned int
	flags);
static int fb_do_read(endpoint_t ep, iovec_t *iov, int minor, u64_t pos,
	size_t *io_bytes);
static int fb_do_write(endpoint_t ep, iovec_t *iov, int minor, u64_t pos,
	size_t *io_bytes);
static int fb_ioctl(message *m);
static void paint_bootlogo(int minor);
static void paint_restartlogo(int minor);
static void paint_centered(int minor, char *data, int width, int height);
static int do_get_varscreeninfo(int minor, endpoint_t ep, cp_grant_id_t gid);
static int do_put_varscreeninfo(int minor, endpoint_t ep, cp_grant_id_t gid);
static int do_get_fixscreeninfo(int minor, endpoint_t ep, cp_grant_id_t gid);
static int do_pan_display(int minor, endpoint_t ep, cp_grant_id_t gid);
static int keep_displaying_restarted(void);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init(int type, sef_init_info_t *info);
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);

/* Entry points to the fb driver. */
static struct chardriver fb_tab =
{
	fb_open,
	fb_close,
	fb_ioctl,
	fb_prepare,
	fb_transfer,
	nop_cleanup,
	nop_alarm,
	nop_cancel,
	nop_select,
	NULL
};

/** Represents the /dev/fb device. */
static struct device fb_device[FB_DEV_NR];
static int fb_minor, has_restarted = 0;
static u64_t has_restarted_t1, has_restarted_t2;

static int open_counter[FB_DEV_NR];		/* Open count */

static int
fb_open(message *m)
{
	static int initialized = 0;

	if (m->DEVICE < 0 || m->DEVICE >= FB_DEV_NR) return ENXIO;

	if (arch_fb_init(m->DEVICE, &fb_device[m->DEVICE]) == OK) {
		open_counter[m->DEVICE]++;
		if (!initialized) {
			if (has_restarted) {
				read_frclock_64(&has_restarted_t1);
				paint_restartlogo(m->DEVICE);
			} else {
				paint_bootlogo(m->DEVICE);
			}
			initialized = 1;
		}
		return OK;
	}
	return ENXIO ;
}

static int
fb_close(message *m)
{
	if (m->DEVICE < 0 || m->DEVICE >= FB_DEV_NR) return ENXIO;
	assert(open_counter[m->DEVICE] > 0);
	open_counter[m->DEVICE]--;
	return OK;
}

static struct device *
fb_prepare(dev_t dev)
{
	if (dev < 0 || dev >= FB_DEV_NR) return NULL;
	assert(open_counter[dev] > 0);
	fb_minor = dev;
	return &fb_device[dev];
}

static int
fb_transfer(endpoint_t endpt, int opcode, u64_t position,
    iovec_t *iov, unsigned nr_req, endpoint_t UNUSED(user_endpt),
    unsigned int UNUSED(flags))
{
	size_t io_bytes = 0, ret;

	if (nr_req != 1) {
		/* This should never trigger for char drivers at the moment. */
		printf("fb: vectored transfer, using first element only\n");
	}

	switch (opcode) {
	case DEV_GATHER_S:
	    /* Userland read operation */
	    ret = fb_do_read(endpt, iov, fb_minor, position, &io_bytes);
	    iov->iov_size -= io_bytes;
            break;
	case DEV_SCATTER_S:
	    /* Userland write operation */
	    ret = fb_do_write(endpt, iov, fb_minor, position, &io_bytes);
	    iov->iov_size -= io_bytes;
	    break;
	default:
	    return EINVAL;
	}
	return ret;
}

static int
fb_do_read(endpoint_t ep, iovec_t *iov, int minor, u64_t pos, size_t *io_bytes)
{
	struct device dev;

	arch_get_device(minor, &dev);

	if (pos >= dev.dv_size) return EINVAL;

	if (dev.dv_size - pos < iov->iov_size) {
		*io_bytes = dev.dv_size - pos;
	} else {
		*io_bytes = iov->iov_size;
	}

        if (*io_bytes <= 0) {
                return OK;
        }

	return sys_safecopyto(ep, (cp_grant_id_t) iov->iov_addr, 0,
				(vir_bytes) (dev.dv_base + ex64lo(pos)),
				*io_bytes);
}

static int
fb_ioctl(message *m)
{
/* Process I/O control requests */
	endpoint_t ep;
	cp_grant_id_t gid;
	int minor;
	unsigned int request;
	int r;

	minor = m->DEVICE;
	request = m->COUNT;
	ep = (endpoint_t) m->USER_ENDPT;
	gid = (cp_grant_id_t) m->IO_GRANT;

	if (minor != 0) return EINVAL;

	switch(request) {
	case FBIOGET_VSCREENINFO:
		r = do_get_varscreeninfo(minor, ep, gid);
		return r;
	case FBIOPUT_VSCREENINFO:
		r = do_put_varscreeninfo(minor, ep, gid);
		return r;
	case FBIOGET_FSCREENINFO:
		r = do_get_fixscreeninfo(minor, ep, gid);
		return r;
	case FBIOPAN_DISPLAY:
		r = do_pan_display(minor, ep, gid);
		return r;
	}

	return EINVAL;
}

static int
do_get_varscreeninfo(int minor, endpoint_t ep, cp_grant_id_t gid)
{
	int r;
	struct fb_var_screeninfo fbvs;

	if ((r = arch_get_varscreeninfo(minor, &fbvs)) == OK) {
		r = sys_safecopyto(ep, gid, 0, (vir_bytes) &fbvs, sizeof(fbvs));
	}

	return r;
}

static int
do_put_varscreeninfo(int minor, endpoint_t ep, cp_grant_id_t gid)
{
	int r;
	struct fb_var_screeninfo fbvs_copy;

	if (has_restarted && keep_displaying_restarted()) {
		return EAGAIN;
	}

	if ((r = sys_safecopyfrom(ep, gid, 0, (vir_bytes) &fbvs_copy,
	    sizeof(fbvs_copy))) != OK) {
		return r;
	}

	return arch_put_varscreeninfo(minor, &fbvs_copy);
}

static int
do_pan_display(int minor, endpoint_t ep, cp_grant_id_t gid)
{
	int r;
        struct fb_var_screeninfo fbvs_copy;

	if (has_restarted && keep_displaying_restarted()) {
		return EAGAIN;
	}

        if ((r = sys_safecopyfrom(ep, gid, 0, (vir_bytes) &fbvs_copy,
            sizeof(fbvs_copy))) != OK) {
                return r;
        }

        return arch_pan_display(minor, &fbvs_copy);
}

static int
do_get_fixscreeninfo(int minor, endpoint_t ep, cp_grant_id_t gid)
{
        int r;
        struct fb_fix_screeninfo fbfs;

        if ((r = arch_get_fixscreeninfo(minor, &fbfs)) == OK) {
                r = sys_safecopyto(ep, gid, 0, (vir_bytes) &fbfs, sizeof(fbfs));
        }

        return r;
}

static int
fb_do_write(endpoint_t ep, iovec_t *iov, int minor, u64_t pos, size_t *io_bytes)
{
	struct device dev;

	arch_get_device(minor, &dev);

	if (pos >= dev.dv_size) {
		return EINVAL;
	}

	if (dev.dv_size - pos < iov->iov_size) {
		*io_bytes = dev.dv_size - pos;
	} else {
		*io_bytes = iov->iov_size;
	}

        if (*io_bytes <= 0) {
                return OK;
        }

	if (has_restarted && keep_displaying_restarted()) {
		return EAGAIN;
	}

	return sys_safecopyfrom(ep, (cp_grant_id_t) iov->iov_addr, 0,
				(vir_bytes) (dev.dv_base + ex64lo(pos)),
				*io_bytes);
}

static int
sef_cb_lu_state_save(int UNUSED(state)) {
/* Save the state. */
	ds_publish_u32("open_counter", open_counter[0], DSF_OVERWRITE);

	return OK;
}

static int
lu_state_restore() {
/* Restore the state. */
	u32_t value;

	ds_retrieve_u32("open_counter", &value);
	ds_delete_u32("open_counter");
	open_counter[0] = (int) value;

	return OK;
}

static void
sef_local_startup()
{
	/* Register init callbacks. Use the same function for all event types */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/* Register live update callbacks  */
	/* - Agree to update immediately when LU is requested in a valid state*/
	sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
	/* - Support live update starting from any standard state */
	sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
	/* - Register a custom routine to save the state. */
	sef_setcb_lu_state_save(sef_cb_lu_state_save);

	/* Let SEF perform startup. */
	sef_startup();
}

static int
sef_cb_init(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the fb driver. */
	int do_announce_driver = TRUE;

	open_counter[0] = 0;
	switch(type) {
	case SEF_INIT_FRESH:
	    printf("framebuffer fresh: pid %d\n", getpid());
	    break;

	case SEF_INIT_LU:
	    /* Restore the state. */
	    lu_state_restore();
	    do_announce_driver = FALSE;

	    printf("framebuffer: I'm a new version!\n");
	    break;

	case SEF_INIT_RESTART:
	    printf("framebuffer restarted: pid %d\n", getpid());
	    has_restarted = 1;
	    break;
	}

	/* Announce we are up when necessary. */
	if (do_announce_driver) {
		chardriver_announce();
	}

	/* Initialization completed successfully. */
	return OK;
}

int
main(void)
{
	sef_local_startup();
	chardriver_task(&fb_tab, CHARDRIVER_SYNC);
	return OK;
}

static int
keep_displaying_restarted()
{
	u64_t delta;
	u32_t micro_delta;

	read_frclock_64(&has_restarted_t2);
	delta = delta_frclock_64(has_restarted_t1, has_restarted_t2);
	micro_delta = frclock_64_to_micros(delta);

#define DISPLAY_1SEC 1000000	/* 1 second in microseconds */
	if (micro_delta < DISPLAY_1SEC) {
		return 1;
	}

	has_restarted = 0;
	return 0;
}

static void
paint_bootlogo(int minor)
{
	paint_centered(minor, bootlogo_data, bootlogo_width, bootlogo_height);
}

static void
paint_restartlogo(int minor)
{
	paint_centered(minor, restartlogo_data, restartlogo_width,
			restartlogo_height);
}

static void
paint_centered(int minor, char *data, int width, int height)
{
	u8_t pixel[3];
	u32_t i, min_x, min_y, max_x, max_y, x_painted = 0, rows = 0;
	int r, bytespp;
	struct device dev;
	struct fb_var_screeninfo fbvs;

	/* Put display in a known state to simplify positioning code below */
	if ((r = arch_get_varscreeninfo(minor, &fbvs)) != OK) {
		printf("fb: unable to get screen info: %d\n", r);
	}
	fbvs.yoffset = 0;
	if ((r = arch_pan_display(minor, &fbvs)) != OK) {
		printf("fb: unable to pan display: %d\n", r);
	}

	arch_get_device(minor, &dev);

	/* Paint on a white canvas */
	bytespp = fbvs.bits_per_pixel / 8;
	for (i = 0; i < fbvs.xres * fbvs.yres * bytespp; i+= bytespp)
		*((u32_t *)((u32_t) dev.dv_base + i)) = 0x00FFFFFF;

	/* First seek to start */
	min_x = fbvs.xres / 2 - width / 2;
	max_x = fbvs.xres / 2 + width / 2;
	min_y = fbvs.yres / 2 - height / 2;
	max_y = fbvs.yres / 2 + height / 2;
	i = min_x * fbvs.xres + min_y;

	/* Add the image data */
	for (i = ((min_y * fbvs.xres) + min_x) * bytespp; rows < height;) {
		GET_PIXEL(data, pixel);

		((unsigned char *)((u32_t) dev.dv_base + i))[0] = pixel[2];
		((unsigned char *)((u32_t) dev.dv_base + i))[1] = pixel[1];
                ((unsigned char *)((u32_t) dev.dv_base + i))[2] = pixel[0];
		((unsigned char *)((u32_t) dev.dv_base + i))[3] = 0;

		x_painted++;/* Keep tab of how many row pixels we've painted */
		if (x_painted == width) {
			/* We've reached the end of the row, carriage return
			 * and go to next line.
			 */
			x_painted = 0;
			rows++;
			i = (((min_y + rows) * fbvs.xres) + min_x) * 4;
		} else {
			i += 4;
		}
	}
}

