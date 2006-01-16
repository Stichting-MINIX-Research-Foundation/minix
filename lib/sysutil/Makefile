#Generated from ./sysutil/Makefile.in
all: all-ack

all-ack:
all-gnu:

makefiles: Makefile

Makefile: Makefile.in
	sh ../generate.sh ./sysutil ../obj-ack ../obj-gnu
	@echo
	@echo "*Attention*"
	@echo "Makefile is regenerated... rerun command to see changes"
	@echo "*Attention*"
	@echo

depend: depend-ack
all-ack: ../obj-ack/libsysutil.a

../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(kmalloc.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(kprintf.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(kputc.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(tickdelay.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(getuptime.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(env_get_prm.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(env_parse.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(env_panic.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(env_prefix.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(fkey_ctl.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(panic.o)
../obj-ack/libsysutil.a: ../obj-ack/libsysutil.a(report.o)

../obj-ack/libsysutil.a:
	ar cr ../obj-ack/libsysutil.a ../obj-ack/./sysutil/*.o
	rm ../obj-ack/./sysutil/*.o

../obj-ack/libsysutil.a(kmalloc.o): kmalloc.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/kmalloc.o kmalloc.c
../obj-ack/libsysutil.a(kprintf.o): kprintf.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/kprintf.o kprintf.c
../obj-ack/libsysutil.a(kputc.o): kputc.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/kputc.o kputc.c
../obj-ack/libsysutil.a(tickdelay.o): tickdelay.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/tickdelay.o tickdelay.c
../obj-ack/libsysutil.a(getuptime.o): getuptime.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/getuptime.o getuptime.c
../obj-ack/libsysutil.a(env_get_prm.o): env_get_prm.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/env_get_prm.o env_get_prm.c
../obj-ack/libsysutil.a(env_parse.o): env_parse.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/env_parse.o env_parse.c
../obj-ack/libsysutil.a(env_panic.o): env_panic.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/env_panic.o env_panic.c
../obj-ack/libsysutil.a(env_prefix.o): env_prefix.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/env_prefix.o env_prefix.c
../obj-ack/libsysutil.a(fkey_ctl.o): fkey_ctl.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/fkey_ctl.o fkey_ctl.c
../obj-ack/libsysutil.a(panic.o): panic.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/panic.o panic.c
../obj-ack/libsysutil.a(report.o): report.c
	cc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-ack/./sysutil/report.o report.c

all-gnu: ../obj-gnu/libsysutil.a

../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/kmalloc.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/kprintf.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/kputc.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/tickdelay.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/getuptime.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/env_get_prm.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/env_parse.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/env_panic.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/env_prefix.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/fkey_ctl.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/panic.o
../obj-gnu/libsysutil.a: ../obj-gnu/./sysutil/report.o

../obj-gnu/libsysutil.a:
	gar cr ../obj-gnu/libsysutil.a $?

../obj-gnu/./sysutil/kmalloc.o: kmalloc.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/kmalloc.o kmalloc.c

../obj-gnu/./sysutil/kprintf.o: kprintf.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/kprintf.o kprintf.c

../obj-gnu/./sysutil/kputc.o: kputc.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/kputc.o kputc.c

../obj-gnu/./sysutil/tickdelay.o: tickdelay.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/tickdelay.o tickdelay.c

../obj-gnu/./sysutil/getuptime.o: getuptime.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/getuptime.o getuptime.c

../obj-gnu/./sysutil/env_get_prm.o: env_get_prm.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/env_get_prm.o env_get_prm.c

../obj-gnu/./sysutil/env_parse.o: env_parse.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/env_parse.o env_parse.c

../obj-gnu/./sysutil/env_panic.o: env_panic.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/env_panic.o env_panic.c

../obj-gnu/./sysutil/env_prefix.o: env_prefix.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/env_prefix.o env_prefix.c

../obj-gnu/./sysutil/fkey_ctl.o: fkey_ctl.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/fkey_ctl.o fkey_ctl.c

../obj-gnu/./sysutil/panic.o: panic.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/panic.o panic.c

../obj-gnu/./sysutil/report.o: report.c
	gcc -O -D_MINIX -D_POSIX_SOURCE -c -o ../obj-gnu/./sysutil/report.o report.c




clean::
	rm -f ../obj-ack/./sysutil/*
	rm -f ../obj-gnu/./sysutil/*

include Makedepend-ack
include .depend-ack

include Makedepend-gnu
include .depend-gnu
