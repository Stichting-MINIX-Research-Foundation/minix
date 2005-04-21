# Makefile for lib/utils.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBUTILS	= ../libutils.a
all:	$(LIBUTILS)

OBJECTS	= \
	$(LIBUTILS)(tick_delay.o) \
	$(LIBUTILS)(get_mon_prm.o) \
	$(LIBUTILS)(env_parse.o) \
	$(LIBUTILS)(env_panic.o) \
	$(LIBUTILS)(env_prefix.o) \
	$(LIBUTILS)(fkey_ctl.o) \
	$(LIBUTILS)(get_proc_nr.o) \
	$(LIBUTILS)(srvr_assert.o) \
	$(LIBUTILS)(srvr_panic.o) \
	$(LIBUTILS)(srvr_report.o) \
	$(LIBUTILS)(taskcall.o) \
	 

$(LIBUTILS):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBUTILS)(tick_delay.o):	tick_delay.c
	$(CC1) tick_delay.c

$(LIBUTILS)(get_mon_prm.o):	get_mon_prm.c
	$(CC1) get_mon_prm.c

$(LIBUTILS)(env_parse.o):	env_parse.c
	$(CC1) env_parse.c

$(LIBUTILS)(env_prefix.o):	env_prefix.c
	$(CC1) env_prefix.c

$(LIBUTILS)(env_panic.o):	env_panic.c
	$(CC1) env_panic.c

$(LIBUTILS)(fkey_ctl.o):	fkey_ctl.c
	$(CC1) fkey_ctl.c

$(LIBUTILS)(get_proc_nr.o):	get_proc_nr.c
	$(CC1) get_proc_nr.c

$(LIBUTILS)(srvr_assert.o):	srvr_assert.c
	$(CC1) srvr_assert.c

$(LIBUTILS)(srvr_panic.o):	srvr_panic.c
	$(CC1) srvr_panic.c

$(LIBUTILS)(srvr_report.o):	srvr_report.c
	$(CC1) srvr_report.c

$(LIBUTILS)(taskcall.o):	taskcall.c
	$(CC1) taskcall.c


