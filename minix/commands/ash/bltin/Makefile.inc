
.PATH: ${.CURDIR}/bltin

SRCS+= echo.c error.c expr.c regexp.c

# LSC Again nbmake seems dumber on MINIX...
expr.c: operators.h
