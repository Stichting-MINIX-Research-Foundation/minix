# stdtime sources
.PATH: ${.CURDIR}/stdtime

Z=${.CURDIR}/../../commands/zoneinfo

CPPFLAGS.asctime.c+=-I$Z
CPPFLAGS.localtime.c+=-I$Z
CPPFLAGS.strftime.c+=-I$Z

SRCS+=  \
	ftime.c \
	asctime.c \
	localtime.c \
	strftime.c
