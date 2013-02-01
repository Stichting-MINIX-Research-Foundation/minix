#ifndef __MINIX_FB_H_
#define __MINIX_FB_H_

#include <minix/type.h>

struct fb_fix_screeninfo {
	char id[16];		/* Identification string */
	u16_t xpanstep;
	u16_t ypanstep;
	u16_t ywrapstep;
	u32_t line_length;
	phys_bytes mmio_start;
	size_t mmio_len;
	u16_t reserved[15];
};

struct fb_bitfield {
	u32_t offset;
	u32_t length;
	u32_t msb_right;
};

struct fb_var_screeninfo {
	u32_t xres;		/* visible resolution */
	u32_t yres;
	u32_t xres_virtual;	/* virtual resolution */
	u32_t yres_virtual;
	u32_t xoffset;		/* offset from virtual to visible */
	u32_t yoffset;
	u32_t bits_per_pixel;
	struct fb_bitfield red;	/* bitfield in fb mem if true color */
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency */
	u16_t reserved[10];
};

#endif /* __MINIX_FB_H_ */
