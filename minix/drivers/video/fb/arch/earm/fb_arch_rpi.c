/* Architecture dependent part for the framebuffer on the OMAP3.
 * There's obvious room for improvement.
 */

#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/fb.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <minix/log.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>
#include <dev/videomode/edidreg.h>
#include "dss.h"
#include "fb.h"

/* default / fallback resolution if EDID reading fails */
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define PAGES_NR 2

#define NSUPPORTED_MODES (4)

/* List of valid modes from TRM 7.1
 * Other modes might work (like the default 1024x600), but no guarantees.
 */
struct supported_modes {
	int hdisplay;
	int vdisplay;
} omap_supported_modes[NSUPPORTED_MODES] = {
	{ .hdisplay = 1024, .vdisplay = 768 }, /* XGA */
	{ .hdisplay = 1280, .vdisplay = 800 }, /* WXGA */
	{ .hdisplay = 1400, .vdisplay = 1050 }, /* SXGA+ */
	{ .hdisplay = 1280, .vdisplay = 720 } /* HD 720p */
};

/* local function prototypes */
static struct videomode *choose_mode(struct edid_info *info);
static void configure_with_defaults(int minor);

/* globals */
static vir_bytes mbox_phys_base;	/* Address of dss phys memory map */
static u32_t* mboxbuffer_vir;	/* Address of dss phys memory map */
static phys_bytes mboxbuffer_phys;	/* Address of dss phys memory map */

static vir_bytes fb_vir;
static phys_bytes fb_phys;
static size_t fb_size;
static int initialized = 0;

#define MAILBOX_BASE     0x3f00b000
#define MAILBOX0_READ    0x880
#define MAILBOX0_STATUS  0x898
#define MAILBOX0_WRITE   0x8a0

struct panel_config {
	u32_t timing_h;
	u32_t timing_v;
	u32_t pol_freq;
	u32_t divisor;
	u32_t lcd_size;
	u32_t panel_type;
	u32_t data_lines;
	u32_t load_mode;
	u32_t panel_color;
};

static const struct fb_fix_screeninfo default_fbfs = {
	.xpanstep	= 0,
	.ypanstep	= 0,
	.ywrapstep	= 0,
	.line_length	= SCREEN_WIDTH * 4,
	.mmio_start	= 0,	/* Not implemented for char. special, so */
	.mmio_len	= 0	/* these are set to 0 */
};

static struct fb_fix_screeninfo omap_fbfs[FB_DEV_NR];

static const struct fb_var_screeninfo default_fbvs = {
	.xres		= SCREEN_WIDTH,
	.yres		= SCREEN_HEIGHT,
	.xres_virtual	= SCREEN_WIDTH,
	.yres_virtual	= SCREEN_HEIGHT,
	.xoffset	= 0,
	.yoffset	= 0,
	.bits_per_pixel = 32,
	.red =	{
		.offset = 16,
		.length = 8,
		.msb_right = 0
		},
	.green = {
		.offset = 8,
		.length = 8,
		.msb_right = 0
		},
	.blue =	{
		.offset = 0,
		.length = 8,
		.msb_right = 0
		},
	.transp = {
		.offset = 24,
		.length = 8,
		.msb_right = 0
		}
};

static struct fb_var_screeninfo omap_fbvs[FB_DEV_NR];

/* logging - use with log_warn(), log_info(), log_debug(), log_trace() */
static struct log log = {
	.name = "fb",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static inline u32_t
readw(vir_bytes addr)
{
	return *((volatile u32_t *) addr);
}

static inline void
writew(vir_bytes addr, u32_t val)
{
	*((volatile u32_t *) addr) = val;
}

static u32_t readmailbox(unsigned channel)
{
	u32_t data;

	while (1) {
		while(readw(mbox_phys_base+MAILBOX0_STATUS) & 0x40000000);

		asm volatile("dmb" : : : "memory");
		data = readw(mbox_phys_base+MAILBOX0_READ);
		asm volatile("dmb" : : : "memory");

		if ((data & 15) == channel)
			return data;
	}
}

static void writemailbox(unsigned channel, u32_t data)
{
	while(readw(mbox_phys_base+MAILBOX0_STATUS) & 0x80000000);

	asm volatile("dmb" : : : "memory");
	writew(mbox_phys_base+MAILBOX0_WRITE, data | channel);
}

static void
configure_with_defaults(int minor)
{
	if (minor < 0 || minor >= FB_DEV_NR) {
		log_warn(&log, "Invalid minor #%d\n", minor);
		return;
	}

	/* copy the default values into this minor's configuration */
	memcpy(&omap_fbfs[minor], &default_fbfs, sizeof(struct fb_fix_screeninfo));
	memcpy(&omap_fbvs[minor], &default_fbvs, sizeof(struct fb_var_screeninfo));
}

static void
arch_configure_display(int minor)
{
/* Tell hardware where frame buffer is and turn display on */
	u32_t off, rdispc;

	struct minix_mem_range mr;

	if (!initialized) return;
	if (minor != 0) return;

	/* Fill mailbox property tags buffer */
	mboxbuffer_vir[0] = 4096;
	mboxbuffer_vir[1] = 0;
	mboxbuffer_vir[2] = 0x00048003; /* set physical size */
	mboxbuffer_vir[3] = 8;
	mboxbuffer_vir[4] = 0;
	mboxbuffer_vir[5] = omap_fbvs[0].xres_virtual;
	mboxbuffer_vir[6] = omap_fbvs[0].yres_virtual;
	mboxbuffer_vir[7] = 0x00048004; /* set virtual size */
	mboxbuffer_vir[8] = 8;
	mboxbuffer_vir[9] = 0;
	mboxbuffer_vir[10] = omap_fbvs[0].xres_virtual;
	mboxbuffer_vir[11] = omap_fbvs[0].yres_virtual;
	mboxbuffer_vir[12] = 0x00048005; /* set depth */
	mboxbuffer_vir[13] = 4;
	mboxbuffer_vir[14] = 0;
	mboxbuffer_vir[15] = omap_fbvs[0].bits_per_pixel;
	mboxbuffer_vir[16] = 0x00040001; /* allocate framebuffer */
	mboxbuffer_vir[17] = 8;
	mboxbuffer_vir[18] = 0;
	mboxbuffer_vir[19] = 4096;
	mboxbuffer_vir[20] = 0; /* end tag */

	writemailbox(8, mboxbuffer_phys + 0x40000000);
	readmailbox(8);

	if (mboxbuffer_vir[1] != 0x80000000)
		panic("Unable to configure framebuffer");

	int c = 2;
	while (mboxbuffer_vir[c] != 0x40001)
		c += 3 + (mboxbuffer_vir[c+1] >> 2);

	if (mboxbuffer_vir[c+2] != 0x80000008)
		panic("Unrecognized response from mailbox");

	/* Configure framebuffer memory access */
	mboxbuffer_vir[c+3] &= ~0xC0000000;
	mr.mr_base = mboxbuffer_vir[c+3];
	mr.mr_limit = mboxbuffer_vir[c+3] + mboxbuffer_vir[c+4];
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		panic("Unable to request access to framebuffer memory");
	}

	fb_size = mr.mr_limit-mr.mr_base;
	fb_vir = (vir_bytes) vm_map_phys(SELF, (void *) mr.mr_base, fb_size);
	if (fb_vir == (vir_bytes) MAP_FAILED) {
		panic("Unable to map framebuffer memory");
	}
}

int
arch_get_device(int minor, struct device *dev)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;
	dev->dv_base = fb_vir;
	dev->dv_size = fb_size;
	return OK;
}

int
arch_get_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;

	*fbvsp = omap_fbvs[minor];
	return OK;
}

int
arch_put_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp)
{
	int r = OK;
	
	assert(fbvsp != NULL);

	if (!initialized) return ENXIO;
	if (minor != 0)	return ENXIO;

	/* For now we only allow to play with the yoffset setting */
	if (fbvsp->yoffset != omap_fbvs[minor].yoffset) {
		if (/* fbvsp->yoffset < 0 || */ fbvsp->yoffset > omap_fbvs[minor].yres) {
			return EINVAL;
		}

		omap_fbvs[minor].yoffset = fbvsp->yoffset;
	}
	
	/* Now update hardware with new settings */
	arch_configure_display(minor);
	return OK;
}

int
arch_get_fixscreeninfo(int minor, struct fb_fix_screeninfo *fbfsp)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;

	*fbfsp = omap_fbfs[minor];
	return OK;
}

int
arch_pan_display(int minor, struct fb_var_screeninfo *fbvsp)
{
	return arch_put_varscreeninfo(minor, fbvsp);
}

int
arch_fb_init(int minor, struct edid_info *info)
{
	int r;
	u32_t rdispc;
	struct minix_mem_range mr;

	if (minor != 0) return ENXIO;	/* We support only one minor */

	if (initialized) {
		return OK;
	} else {
		log_debug(&log, "Loading Default Settings...\n");
		configure_with_defaults(minor);
	}

	initialized = 1;

	/* Configure mailbox memory access */
	mr.mr_base = MAILBOX_BASE;
	mr.mr_limit = mr.mr_base + 0x1000;
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		panic("Unable to request access to mailbox memory");
	}

	mbox_phys_base = (vir_bytes) vm_map_phys(SELF, (void *) MAILBOX_BASE,
						0x1000);

	if (mbox_phys_base == (vir_bytes) MAP_FAILED) {
		panic("Unable to map mailbox memory");
	}

	/* Configure mailbox buffer */
	mboxbuffer_vir = (u32_t*) alloc_contig(0x1000, 0, &mboxbuffer_phys);
	if (mboxbuffer_vir == (u32_t*) MAP_FAILED) {
		panic("Unable to allocate contiguous memory for mailbox buffer\n");
	}

	/* Configure buffer settings and turn on LCD/Digital */
	arch_configure_display(minor);

	return OK;
}

