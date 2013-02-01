#ifndef __FB_H__
#define __FB_H__

int arch_fb_init(int minor, struct device *dev);
int arch_get_device(int minor, struct device *dev);
int arch_get_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp);
int arch_put_varscreeninfo(int minor, struct fb_var_screeninfo *fbvs_copy);
int arch_get_fixscreeninfo(int minor, struct fb_fix_screeninfo *fbfsp);
int arch_pan_display(int minor, struct fb_var_screeninfo *fbvs_copy);

#define FB_MESSAGE "Hello, world! From framebuffer!\n"

#endif /* __FB_H__ */
