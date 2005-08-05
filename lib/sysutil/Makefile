# Makefile for lib/utils.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBUTILS	= ../libsysutil.a
all:	$(LIBUTILS)

OBJECTS	= \
	$(LIBUTILS)(kmalloc.o) \
	$(LIBUTILS)(kprintf.o) \
	$(LIBUTILS)(kputc.o) \
	$(LIBUTILS)(tickdelay.o) \
	$(LIBUTILS)(getuptime.o) \
	$(LIBUTILS)(env_get_prm.o) \
	$(LIBUTILS)(env_parse.o) \
	$(LIBUTILS)(env_panic.o) \
	$(LIBUTILS)(env_prefix.o) \
	$(LIBUTILS)(fkey_ctl.o) \
	$(LIBUTILS)(panic.o) \
	$(LIBUTILS)(report.o) \
	$(LIBUTILS)(taskcall.o) \
	 

$(LIBUTILS):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBUTILS)(kmalloc.o):	kmalloc.c
	$(CC1) kmalloc.c

$(LIBUTILS)(kprintf.o):	kprintf.c
	$(CC1) kprintf.c

$(LIBUTILS)(kputc.o):	kputc.c
	$(CC1) kputc.c

$(LIBUTILS)(getuptime.o):	getuptime.c
	$(CC1) getuptime.c

$(LIBUTILS)(tickdelay.o):	tickdelay.c
	$(CC1) tickdelay.c

$(LIBUTILS)(env_get_prm.o):	env_get_prm.c
	$(CC1) env_get_prm.c

$(LIBUTILS)(env_parse.o):	env_parse.c
	$(CC1) env_parse.c

$(LIBUTILS)(env_prefix.o):	env_prefix.c
	$(CC1) env_prefix.c

$(LIBUTILS)(env_panic.o):	env_panic.c
	$(CC1) env_panic.c

$(LIBUTILS)(fkey_ctl.o):	fkey_ctl.c
	$(CC1) fkey_ctl.c

$(LIBUTILS)(panic.o):		panic.c
	$(CC1) panic.c

$(LIBUTILS)(report.o):		report.c
	$(CC1) report.c

$(LIBUTILS)(taskcall.o):	taskcall.c
	$(CC1) taskcall.c


