# Makefile for system library implementation

# Directories
u = /usr
i = $u/include

# Programs, flags, etc.
CC =	exec cc $(CFLAGS) -c
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -I$i
LDFLAGS = -i

SYSTEM	= ../system.a

# What to make.
all build install: $(SYSTEM)

OBJECTS	= \
	$(SYSTEM)(do_unused.o) \
	$(SYSTEM)(do_fork.o) \
	$(SYSTEM)(do_exec.o) \
	$(SYSTEM)(do_newmap.o) \
	$(SYSTEM)(do_exit.o) \
	$(SYSTEM)(do_trace.o) \
	$(SYSTEM)(do_nice.o) \
	$(SYSTEM)(do_times.o) \
	$(SYSTEM)(do_setalarm.o) \
	$(SYSTEM)(do_irqctl.o) \
	$(SYSTEM)(do_devio.o) \
	$(SYSTEM)(do_vdevio.o) \
	$(SYSTEM)(do_int86.o) \
	$(SYSTEM)(do_sdevio.o) \
	$(SYSTEM)(do_copy.o) \
	$(SYSTEM)(do_vcopy.o) \
	$(SYSTEM)(do_umap.o) \
	$(SYSTEM)(do_memset.o) \
	$(SYSTEM)(do_privctl.o) \
	$(SYSTEM)(do_segctl.o) \
	$(SYSTEM)(do_getksig.o) \
	$(SYSTEM)(do_endksig.o) \
	$(SYSTEM)(do_kill.o) \
	$(SYSTEM)(do_sigsend.o) \
	$(SYSTEM)(do_sigreturn.o) \
	$(SYSTEM)(do_abort.o) \
	$(SYSTEM)(do_getinfo.o) \

$(SYSTEM):	$(OBJECTS)
	aal cr $@ *.o

clean:
	rm -f $(SYSTEM) *.o *~ *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend


$(SYSTEM)(do_unused.o):	do_unused.c
	$(CC) do_unused.c

$(SYSTEM)(do_fork.o):	do_fork.c
	$(CC) do_fork.c

$(SYSTEM)(do_exec.o):	do_exec.c
	$(CC) do_exec.c

$(SYSTEM)(do_newmap.o):	do_newmap.c
	$(CC) do_newmap.c

$(SYSTEM)(do_exit.o):	do_exit.c
	$(CC) do_exit.c

$(SYSTEM)(do_trace.o):	do_trace.c
	$(CC) do_trace.c

$(SYSTEM)(do_nice.o):	do_nice.c
	$(CC) do_nice.c

$(SYSTEM)(do_times.o):	do_times.c
	$(CC) do_times.c

$(SYSTEM)(do_setalarm.o):	do_setalarm.c
	$(CC) do_setalarm.c

$(SYSTEM)(do_irqctl.o):	do_irqctl.c
	$(CC) do_irqctl.c

$(SYSTEM)(do_devio.o):	do_devio.c
	$(CC) do_devio.c

$(SYSTEM)(do_sdevio.o):	do_sdevio.c
	$(CC) do_sdevio.c

$(SYSTEM)(do_vdevio.o):	do_vdevio.c
	$(CC) do_vdevio.c

$(SYSTEM)(do_int86.o):	do_int86.c
	$(CC) do_int86.c

$(SYSTEM)(do_copy.o):	do_copy.c
	$(CC) do_copy.c

$(SYSTEM)(do_vcopy.o):	do_vcopy.c
	$(CC) do_vcopy.c

$(SYSTEM)(do_umap.o):	do_umap.c
	$(CC) do_umap.c

$(SYSTEM)(do_memset.o):	do_memset.c
	$(CC) do_memset.c

$(SYSTEM)(do_getksig.o):	do_getksig.c
	$(CC) do_getksig.c

$(SYSTEM)(do_endksig.o):	do_endksig.c
	$(CC) do_endksig.c

$(SYSTEM)(do_kill.o):	do_kill.c
	$(CC) do_kill.c

$(SYSTEM)(do_sigsend.o):	do_sigsend.c
	$(CC) do_sigsend.c

$(SYSTEM)(do_sigreturn.o):	do_sigreturn.c
	$(CC) do_sigreturn.c

$(SYSTEM)(do_getinfo.o):	do_getinfo.c
	$(CC) do_getinfo.c

$(SYSTEM)(do_abort.o):	do_abort.c
	$(CC) do_abort.c

$(SYSTEM)(do_privctl.o):	do_privctl.c
	$(CC) do_privctl.c

$(SYSTEM)(do_segctl.o):	do_segctl.c
	$(CC) do_segctl.c
