.include <bsd.own.mk>

.PATH: ${MINIXSRCDIR}/common/include/sys

INCS+= 	ioc_cmos.h ioc_file.h ioc_scsi.h ioc_tape.h \
	ioc_disk.h ioc_memory.h ioc_sound.h ioc_tty.h \
	kbdio.h mtio.h svrctl.h video.h vm.h

