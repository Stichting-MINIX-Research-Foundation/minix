PROG = devmand 

SRCS = main.c lex.yy.c y.tab.c

lex.yy.c: usb_scan.l y.tab.h
	${LEX} usb_scan.l

y.tab.c y.tab.h: usb.y
	${YACC} -d usb.y

CLEANFILES = y.tab.c y.tab.h *.d lex.yy.c lex.yy.o main.o y.tab.o
MAN =
.include <bsd.prog.mk>


