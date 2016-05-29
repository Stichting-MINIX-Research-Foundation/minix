#
# BSP for RPI hardware
#

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/bsp/${BSP_NAME}

.for unpaged_obj in rpi_serial.o rpi_reset.o
BSP_OBJS_UNPAGED += ${unpaged_obj}
.endfor

SRCS+=	rpi_init.c rpi_serial.c rpi_timer.c rpi_padconf.c rpi_intr.c \
	rpi_reset.c
