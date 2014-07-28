#
# BSP for TI hardware
#

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/bsp/ti

.for unpaged_obj in omap_serial.o omap_rtc.o omap_reset.o 
BSP_OBJS_UNPAGED += ${unpaged_obj}
.endfor

SRCS+=	omap_init.c omap_serial.c omap_timer.c omap_padconf.c omap_intr.c omap_rtc.c \
	omap_reset.c

