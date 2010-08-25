# Makefile for the Sound Blaster 16 (SB16) driver

PROGS=sb16_mixer sb16_dsp

SRCS.sb16_dsp=sb16.c sb16_dsp.c sb16_dsp_liveupdate.c
SRCS.sb16_mixer=sb16.c sb16_mixer.c
MAN.sb16_dsp=
MAN.sb16_mixer=

DPADD+= ${LIBDRIVER} ${LIBSYS}
LDADD+= -ldriver -lsys

.include <bsd.prog.mk>
