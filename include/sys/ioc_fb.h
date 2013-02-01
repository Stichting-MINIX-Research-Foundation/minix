/*	sys/ioc_fb.h - Framebuffer command codes
 *
 */

#ifndef _S_I_FB_H
#define _S_I_FB_H

/* The I/O control requests. */
#define FBIOGET_VSCREENINFO	_IOR('V', 1, struct fb_var_screeninfo)
#define FBIOPUT_VSCREENINFO     _IOW('V', 2, struct fb_var_screeninfo)
#define FBIOGET_FSCREENINFO     _IOR('V', 3, struct fb_fix_screeninfo)
#define FBIOPAN_DISPLAY	_IOW('V', 4, struct fb_var_screeninfo)

#endif /* _S_I_FB_H */
