DBG=-O0 -g
CFLAGS+= -Wall -Werror -D_NETBSD_SOURCE -fno-builtin -D_MINIX -D_POSIX_SOURCE
LDADD+= -lm -lcompat_minix

# Tests have no manpages
MKMAN=no

# They are all bin-owned; by default normal executable mode
BINOWN=root

# Some have special flags compiling
CFLAGS.test51=-mhard-float
CFLAGS.test52=-mhard-float

# Some have special libraries
LDADD.test59= -lmthread
LDFLAGS.mod= -shared	# make shared object

# Some have an extra file
OBJS.test57=test57loop.o

.for t in \
 1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 \
21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 \
41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 \
61 62    64 65
PROG+= test$(t)
.endfor
  
PROG+= t10a t11a t11b t40a t40b t40c t40d t40e t40f t60a t60b 

.include <bsd.own.mk>

.if $(MKPIC) == "yes"
# Build them as dynamic executables by default if shared libraries
# are available; so that the building and executing of dynamic
# executables is tested
LDSTATIC=	-dynamic

# Add test that must be linked dynamically, and its dynamically loaded
# module
PROG+= test63 mod
.endif

.include <bsd.prog.mk>

# Some are suid-root
all:
	chmod 4755 test11 test33 test43 test44 test46 test56 test60 test61 \
		   test65

clean: .PHONY .MAKE
	$(MAKE) -C select clean
	rm -rf *.o *.s *.bak test? test?? t10a t11a t11b \
		t40a t40b t40c t40d t40e t40f \
		t60a t60b \
		DIR*
