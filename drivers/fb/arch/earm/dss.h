#ifndef __DSS_H__
#define __DSS_H__ 

/* DSS Base Registers */
#define OMAP3_DSS_BASE			0x48050000
#define OMAP3_DISPC_BASE		0x48050400
#define OMAP3_VENC_BASE			0x48050C00

#define OMAP3_DSS_SYSCONFIG(b)		(b + 0x10)
#define OMAP3_DSS_SYSSTATUS(b)		(b + 0x14)

#define OMAP3_DISPC_IRQSTATUS(b)	(b + 0x18)
#define OMAP3_DISPC_CONTROL(b)		(b + 0x40)
#define OMAP3_DISPC_CONFIG(b)		(b + 0x44)
#define OMAP3_DISPC_DEFAULT_COLOR0(b)	(b + 0x4c)
#define OMAP3_DISPC_DEFAULT_COLOR1(b)	(b + 0x50)
#define OMAP3_DISPC_TIMINGH(b)		(b + 0x64)
#define OMAP3_DISPC_TIMINGV(b)		(b + 0x68)
#define OMAP3_DISPC_POL_FREQ(b)		(b + 0x6c)
#define OMAP3_DISPC_DIVISOR(b)		(b + 0x70)
#define OMAP3_DISPC_SIZE_DIG(b)		(b + 0x78)
#define OMAP3_DISPC_SIZE_LCD(b)		(b + 0x7c)
#define OMAP3_DISPC_GFX_BA0(b)		(b + 0x80)
#define OMAP3_DISPC_GFX_BA1(b)		(b + 0x84)
#define OMAP3_DISPC_GFX_SIZE(b)		(b + 0x8c)
#define OMAP3_DISPC_GFX_ATTRIBUTES(b)	(b + 0xa0)
#define OMAP3_DISPC_GFX_ROW_INC(b)	(b + 0xac)
#define OMAP3_DISPC_GFX_PIXEL_INC(b)	(b + 0xb0)

#define LOADMODE_SHIFT		1
#define TFTSTN_SHIFT		3
#define DATALINES_SHIFT		8
#define GFXFORMAT_SHIFT		1
#define GFXBURSTSIZE_SHIFT	6

#define DSS_SOFTRESET			(1 << 1)
#define DSS_RESETDONE			(1 << 0)

#define DISPC_LCDENABLE			(1 << 0)
#define DISPC_DIGITALENABLE		(1 << 1)
#define DISPC_GOLCD			(1 << 5)
#define DISPC_GODIGITAL			(1 << 6)
#define DISPC_GPIN0			(1 << 13)
#define DISPC_GPIN1			(1 << 14)
#define DISPC_GPOUT0			(1 << 15)
#define DISPC_GPOUT1			(1 << 16)
#define DISPC_ENABLESIGNAL		(1 << 28)
#define DISPC_FRAMEDONE			(1 << 0)
#define DISPC_GFXENABLE			(1 << 0)
#define DISPC_GFXFORMAT_BMP1		0x0
#define DISPC_GFXFORMAT_BMP2		0x1
#define DISPC_GFXFORMAT_BMP4		0x2
#define DISPC_GFXFORMAT_BMP8		0x3
#define DISPC_GFXFORMAT_RGB12		0x4
#define DISPC_GFXFORMAT_ARGB16		0x5
#define DISPC_GFXFORMAT_RGB16		0x6
#define DISPC_GFXFORMAT_RGB24		0x8
#define DISPC_GFXFORMAT_RGB24P		0x9
#define DISPC_GFXFORMAT_ARGB32		0xC
#define DISPC_GFXFORMAT_RGBA32		0xD
#define DISPC_GFXFORMAT_RGBx		0xE
#define DISPC_GFXBURSTSIZE_4		0x0
#define DISPC_GFXBURSTSIZE_8		0x1
#define DISPC_GFXBURSTSIZE_16		0x2
#endif /* __DSS_H__ */
