.text

.include "releasetools/rpi-bootloader/bootloader_common.S"

.align 4
argv:
	.word	argv0
	.word	argv1
	.word	0x0
argv0:
	.asciz "0x00200000"
argv1:
	.asciz "board_name=RPI_2_B console=tty00"

.data
.align 4
	.space 256
stack_top:
