
X=a b c d e

.for x in $X
LIB${x:tu}=/tmp/lib$x.a
.endfor

X_LIBS= ${LIBA} ${LIBD} ${LIBE}

LIB?=a

all:
	@for x in $X; do ${.MAKE} -f ${MAKEFILE} show LIB=$$x; done

show:
	@echo 'LIB=${LIB} X_LIBS:M$${LIB$${LIB:tu}} is "${X_LIBS:M${LIB${LIB:tu}}}"'
	@echo 'LIB=${LIB} X_LIBS:M*/lib$${LIB}.a is "${X_LIBS:M*/lib${LIB}.a}"'
	@echo 'LIB=${LIB} X_LIBS:M*/lib$${LIB}.a:tu is "${X_LIBS:M*/lib${LIB}.a:tu}"'
