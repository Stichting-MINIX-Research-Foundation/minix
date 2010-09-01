# Makefile for the tests.

GCC=gcc
CFLAGS= -O0 -D_MINIX -D_POSIX_SOURCE
CFLAGS-GCC= $(CFLAGS) -Wall 
CFLAGS-GCCFPU= $(CFLAGS) -Wall -mhard-float
CFLAGS-GCCFPU= $(CFLAGS-GCC) -W -mhard-float

OBJ=	test1  test2  test3  test4  test5  test6  test7  test8  test9  \
	test10        test12 test13 test14 test15 test16 test17 test18 test19 \
	       test21 test22 test23        test25 test26 test27 test28 test29 \
	test30 test31 test32        test34 test35 test36 test37 test38 \
	test39 t10a t11a t11b test40 t40a t40b t40c t40d t40e t40f test41 \
	test42 test45 test47 test48 test49 test50 test51 test52 test53 \
	test54 test55 test56 test58

BIGOBJ=  test20 test24
ROOTOBJ= test11 test33 test43 test44 test46
GCCOBJ=  test45-gcc test49-gcc 
GCCFPUOBJ= test51-gcc test52-gcc

all:	$(OBJ) $(BIGOBJ) $(GCCOBJ) $(GCCFPUOBJ) $(ROOTOBJ) test57
	chmod 755 *.sh run

$(OBJ):
	$(CC) $(CFLAGS) -o $@ $@.c

$(BIGOBJ):
	$(CC) $(CFLAGS) -o $@ $@.c

$(GCCOBJ):
	if which $(GCC) >/dev/null 2>&1; then $(GCC) $(CFLAGS-GCC) -o $@ ${@:S/-gcc//}.c; fi

$(GCCFPUOBJ):
	if which $(GCC) >/dev/null 2>&1; then $(GCC) $(CFLAGS-GCCFPU) -o $@ ${@:S/-gcc//}.c; fi

$(ROOTOBJ):
	$(CC) $(CFLAGS) $@.c
	@install -c -o root -m 4755 a.out $@
	@rm a.out

clean:	
	$(MAKE) -C select clean
	-rm -rf *.o *.s *.bak test? test?? test??-gcc t10a t11a t11b \
		t40a t40b t40c t40d t40e t40f t43 DIR*

test1:	test1.c
test2:	test2.c
test3:	test3.c
test4:	test4.c
test5:	test5.c
test6:	test6.c
test7:	test7.c
test8:	test8.c
test9:	test9.c
test10:	test10.c
t10a:	t10a.c
test11:	test11.c
t11a:	t11a.c
t11b:	t11b.c
test12:	test12.c
test13:	test13.c
test14:	test14.c
test15:	test15.c
test16:	test16.c
test17:	test17.c
test18:	test18.c
test19:	test19.c
test20:	test20.c
test21:	test21.c
test22:	test22.c
test23:	test23.c
test24:	test24.c
test25:	test25.c
test26:	test26.c
test27:	test27.c
test28:	test28.c
test29:	test29.c
test30:	test30.c
test31:	test31.c
test32:	test32.c
test33:	test33.c
test34:	test34.c
test35:	test35.c
test36:	test36.c
test37:	test37.c
test38:	test38.c
test39: test39.c
test40: test40.c
t40a: t40a.c
t40b: t40b.c
t40c: t40c.c
t40d: t40d.c
t40e: t40e.c
t40f: t40f.c
test41: test41.c
test42: test42.c
test43: test43.c
test44: test44.c
test45: test45.c test45.h
test45-gcc: test45.c test45.h
test46: test46.c
test47: test47.c
test48: test48.c
test49: test49.c
test49-gcc: test49.c
test50: test50.c
test51: test51.c
test51-gcc: test51.c
test52: test52.c
test52-gcc: test52.c
test54: test54.c
test55: test55.c
test56: test56.c
test57: test57.c test57loop.S
	if which $(GCC) >/dev/null 2>&1; then $(GCC) $(CFLAGS-GCC) -o $@ test57.c test57loop.S; fi
test58: test58.c
