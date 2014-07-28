
PROG = devmand 
MAN =

SRCS = main.c usb_scan.l usb.y
CPPFLAGS+= -I${.CURDIR}

YFLAGS:= -d
CLEANFILES+= y.tab.h

usb_scan.l: usb.y

.include <bsd.prog.mk>

# LSC: Seems that this file is implicitly taken into account by NetBSD's make, 
# still seems to be ignored / not found currently.
# It's a sad story, as it has default rules to manage yacc / lex files. So for
# a happy ending here it is explicitly included:
.include <sys.mk>

