/* Architecture dependent part for the framebuffer on the OMAP3. Since we don't
 * have support for EDID (which requires support for i2c, also something we
 * don't have, yet), but we do have a screen with 1024*600 resolution for our
 * testing purposes, we hardcode that resolution here. There's obvious room for
 * improvement. */

#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/fb.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include "dss.h"
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 600
#define PAGES_NR 2

static vir_bytes dss_phys_base;		/* Address of dss phys memory map */
static vir_bytes dispc_phys_base;	/* Address of dispc phys memory map */
static vir_bytes fb_vir;
static phys_bytes fb_phys;
static size_t fb_size;
static int initialized = 0;

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

static const struct panel_config default_cfg = {
	/* See OMAP TRM section 15.7 for the register values/encoding */
        .timing_h       = 0x1a4024c9,	/* Horizontal timing */
        .timing_v       = 0x02c00509,	/* Vertical timing */
        .pol_freq       = 0x00007028,	/* Pol Freq */
        .divisor        = 0x00010001,	/* 96MHz Pixel Clock */
	.lcd_size	= ((SCREEN_HEIGHT - 1) << 16 | (SCREEN_WIDTH - 1)),
        .panel_type     = 0x01,		/* TFT */
        .data_lines     = 0x03,		/* 24 Bit RGB */
        .load_mode      = 0x02,		/* Frame Mode */
        .panel_color    = 0xFFFFFF	/* WHITE */
};

static const struct fb_fix_screeninfo fbfs = {
	.xpanstep	= 0,
	.ypanstep	= 0,
	.ywrapstep	= 0,
	.line_length	= SCREEN_WIDTH * 4,
	.mmio_start	= 0,	/* Not implemented for char. special, so */
	.mmio_len	= 0	/* these are set to 0 */
};

static struct fb_var_screeninfo fbvs = {
	.xres		= SCREEN_WIDTH,
	.yres		= SCREEN_HEIGHT,
	.xres_virtual	= SCREEN_WIDTH,
	.yres_virtual	= SCREEN_HEIGHT*2,
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

static void
arch_configure_display(int minor)
{
/* Tell hardware where frame buffer is and turn display on */
	u32_t off, rdispc;

	if (!initialized) return;
	if (minor != 0) return;

	off = fbvs.yoffset * fbvs.xres_virtual * (fbvs.bits_per_pixel/8);

	writew((vir_bytes) OMAP3_DISPC_GFX_BA0(dispc_phys_base),
		fb_phys + (phys_bytes) off);
	rdispc = readw((vir_bytes) OMAP3_DISPC_CONTROL(dispc_phys_base));
	rdispc |= DISPC_LCDENABLE | DISPC_DIGITALENABLE | DISPC_GOLCD |
				DISPC_GODIGITAL | DISPC_GPOUT0 | DISPC_GPOUT1;
	writew((vir_bytes) OMAP3_DISPC_CONTROL(dispc_phys_base), rdispc);
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

	*fbvsp = fbvs;
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
	if (fbvsp->yoffset != fbvs.yoffset) {
		if (fbvsp->yoffset < 0 || fbvsp->yoffset > fbvs.yres) {
			return EINVAL;
		}

		fbvs.yoffset = fbvsp->yoffset;
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

	*fbfsp = fbfs;
	return OK;
}

int
arch_pan_display(int minor, struct fb_var_screeninfo *fbvsp)
{
	return arch_put_varscreeninfo(minor, fbvsp);
}

int
arch_fb_init(int minor, struct device *dev)
{
	u32_t rdispc;
	struct minix_mem_range mr;

	const struct panel_config *panel_cfg = &default_cfg;

	assert(dev != NULL);
	if (minor != 0) return ENXIO;	/* We support only one minor */

	if (initialized) {
		dev->dv_base = fb_vir;
		dev->dv_size = fb_size;
		return OK;
	}

	initialized = 1;

        /* Configure DSS memory access */
        mr.mr_base = OMAP3_DSS_BASE;
        mr.mr_limit = mr.mr_base + 0x60;
        if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
                panic("Unable to request access to DSS(1) memory");
        }

        dss_phys_base = (vir_bytes) vm_map_phys(SELF, (void *) OMAP3_DSS_BASE,
						0x60);

        if (dss_phys_base == (vir_bytes) MAP_FAILED) {
                panic("Unable to request access to DSS(2) memory");
        }

        /* Configure DISPC memory access */
        mr.mr_base = OMAP3_DISPC_BASE;
        mr.mr_limit = mr.mr_base + 0x430;
        if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
                panic("Unable to request access to DISPC(1) memory");
        }
        dispc_phys_base = (vir_bytes) vm_map_phys(SELF,
						  (void *) OMAP3_DISPC_BASE,
						  0x430);

        if (dispc_phys_base == (vir_bytes) MAP_FAILED) {
                panic("Unable to request access to DISPC(2) memory");
        }

	/* Set timings, screen mode, screen size, etc. */
	writew(OMAP3_DISPC_TIMINGH(dispc_phys_base), panel_cfg->timing_h);
	writew(OMAP3_DISPC_TIMINGV(dispc_phys_base), panel_cfg->timing_v);
	writew(OMAP3_DISPC_POL_FREQ(dispc_phys_base), panel_cfg->pol_freq);
	writew(OMAP3_DISPC_DIVISOR(dispc_phys_base), panel_cfg->divisor);
	writew(OMAP3_DISPC_CONFIG(dispc_phys_base),
				panel_cfg->load_mode << LOADMODE_SHIFT);
	writew(OMAP3_DISPC_CONTROL(dispc_phys_base),
				panel_cfg->panel_type << TFTSTN_SHIFT |
				panel_cfg->data_lines << DATALINES_SHIFT);
	
	writew((vir_bytes) OMAP3_DISPC_SIZE_LCD(dispc_phys_base),
				panel_cfg->lcd_size);
	writew((vir_bytes) OMAP3_DISPC_GFX_SIZE(dispc_phys_base),
				panel_cfg->lcd_size);
	writew(OMAP3_DISPC_DEFAULT_COLOR0(dispc_phys_base),
				panel_cfg->panel_color);

	/* Enable gfx engine */
        writew(OMAP3_DISPC_GFX_ATTRIBUTES(dispc_phys_base),
				(DISPC_GFXBURSTSIZE_16 << GFXBURSTSIZE_SHIFT) |
				(DISPC_GFXFORMAT_RGB24 << GFXFORMAT_SHIFT) |
				(DISPC_GFXENABLE));
	writew(OMAP3_DISPC_GFX_ROW_INC(dispc_phys_base), 1);
	writew(OMAP3_DISPC_GFX_PIXEL_INC(dispc_phys_base), 1);

	/* Allocate contiguous physical memory for the display buffer */
	fb_size = fbvs.yres_virtual * fbvs.xres_virtual *
				(fbvs.bits_per_pixel / 8);
	fb_vir = (vir_bytes) alloc_contig(fb_size, 0, &fb_phys);
	if (fb_vir == (vir_bytes) MAP_FAILED) {
		panic("Unable to allocate contiguous memory\n");
	} 
	dev->dv_base = fb_vir;
	dev->dv_size = fb_size;

	/* Configure buffer settings and turn on LCD/Digital */
	arch_configure_display(minor);

	return OK;
}

