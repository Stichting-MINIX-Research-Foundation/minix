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
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 600
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
static int configure_with_edid(int minor, struct edid_info *info);

/* globals */
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

static struct panel_config omap_cfg[FB_DEV_NR];

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

static struct videomode *
choose_mode(struct edid_info *info)
{
	int i, j;

	/* choose the highest resolution supported by both the SoC and screen */
	for (i = info->edid_nmodes - 1; i >= 0; i--) {
		for (j = NSUPPORTED_MODES - 1; j >= 0; j--) {

			if (info->edid_modes[i].hdisplay ==
				omap_supported_modes[j].hdisplay &&
				info->edid_modes[i].vdisplay ==
				omap_supported_modes[j].vdisplay) {

				return &(info->edid_modes[i]);
			}
		}
	}

	return NULL;
}

static int
configure_with_edid(int minor, struct edid_info *info)
{
	struct videomode *mode;

	if (info == NULL || minor < 0 || minor >= FB_DEV_NR) {
		log_warn(&log, "Invalid minor #%d or info == NULL\n", minor);
		return -1;
	}

	/* If debugging or tracing, print the contents of info */
	if (log.log_level >= LEVEL_DEBUG) {
		log_debug(&log, "--- EDID - START ---\n");
		edid_print(info);
		log_debug(&log, "--- EDID - END ---\n");
	}

	/* Choose the preferred mode. */
	mode = choose_mode(info);
	if (mode == NULL) {
		log_warn(&log, "Couldn't find a supported resolution.\n");
		return -1;
	}

	/*
	 * apply the default settings since we don't overwrite every field
	 */
	configure_with_defaults(minor);

	/*
	 * apply the settings corresponding to the given EDID
	 */

	/* panel_config */
	omap_cfg[minor].lcd_size    = ((mode->vdisplay - 1) << 16 | (mode->hdisplay - 1));

	if (EDID_FEATURES_DISP_TYPE(info->edid_features) ==
			EDID_FEATURES_DISP_TYPE_MONO) {
		omap_cfg[minor].panel_type  = 0x00;		/* Mono */
	} else {
		omap_cfg[minor].panel_type  = 0x01;		/* RGB/Color */
	}

	/* fb_fix_screeninfo */
	omap_fbfs[minor].line_length = mode->hdisplay * 4;

	/* fb_var_screeninfo */
	omap_fbvs[minor].xres		= mode->hdisplay;
	omap_fbvs[minor].yres		= mode->vdisplay;
	omap_fbvs[minor].xres_virtual	= mode->hdisplay;
	omap_fbvs[minor].yres_virtual	= mode->vdisplay*2;

	return OK;
}

static void
configure_with_defaults(int minor)
{
	if (minor < 0 || minor >= FB_DEV_NR) {
		log_warn(&log, "Invalid minor #%d\n", minor);
		return;
	}

	/* copy the default values into this minor's configuration */
	memcpy(&omap_cfg[minor], &default_cfg, sizeof(struct panel_config));
	memcpy(&omap_fbfs[minor], &default_fbfs, sizeof(struct fb_fix_screeninfo));
	memcpy(&omap_fbvs[minor], &default_fbvs, sizeof(struct fb_var_screeninfo));
}

static void
arch_configure_display(int minor)
{
/* Tell hardware where frame buffer is and turn display on */
	u32_t off, rdispc;

	if (!initialized) return;
	if (minor != 0) return;

	off = omap_fbvs[minor].yoffset * omap_fbvs[minor].xres_virtual * (omap_fbvs[minor].bits_per_pixel/8);

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

	const struct panel_config *panel_cfg = &omap_cfg[minor];

	if (minor != 0) return ENXIO;	/* We support only one minor */

	if (initialized) {
		return OK;
	} else if (info != NULL) {
		log_debug(&log, "Configuring Settings based on EDID...\n");
		r = configure_with_edid(minor, info);
		if (r != OK) {
			log_warn(&log, "EDID config failed. Using defaults.\n");
			configure_with_defaults(minor);
		}
	} else {
		log_debug(&log, "Loading Default Settings...\n");
		configure_with_defaults(minor);
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
	fb_size = omap_fbvs[minor].yres_virtual * omap_fbvs[minor].xres_virtual *
				(omap_fbvs[minor].bits_per_pixel / 8);
	fb_vir = (vir_bytes) alloc_contig(fb_size, 0, &fb_phys);
	if (fb_vir == (vir_bytes) MAP_FAILED) {
		panic("Unable to allocate contiguous memory\n");
	} 

	/* Configure buffer settings and turn on LCD/Digital */
	arch_configure_display(minor);

	return OK;
}

