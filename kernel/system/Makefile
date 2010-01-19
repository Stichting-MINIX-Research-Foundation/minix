# Makefile for system library implementation

include /etc/make.conf

# Directories
u = /usr
i = $u/include
a = ../arch

# Programs, flags, etc.
CC =	exec cc $(CFLAGS) -c
CCNOPROF =      exec cc $(CFLAGSNOPROF) -c      # no call profiling for these
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -I$i $(CPROFILE) -I$a/$(ARCH)/include
CFLAGSNOPROF = -I$i
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
	$(SYSTEM)(do_runctl.o) \
	$(SYSTEM)(do_times.o) \
	$(SYSTEM)(do_setalarm.o) \
	$(SYSTEM)(do_stime.o) \
	$(SYSTEM)(do_vtimer.o) \
	$(SYSTEM)(do_irqctl.o) \
	$(SYSTEM)(do_devio.o) \
	$(SYSTEM)(do_vdevio.o) \
	$(SYSTEM)(do_copy.o) \
	$(SYSTEM)(do_umap.o) \
	$(SYSTEM)(do_memset.o) \
	$(SYSTEM)(do_setgrant.o) \
	$(SYSTEM)(do_privctl.o) \
	$(SYSTEM)(do_segctl.o) \
	$(SYSTEM)(do_safecopy.o) \
	$(SYSTEM)(do_safemap.o) \
	$(SYSTEM)(do_sysctl.o) \
	$(SYSTEM)(do_getksig.o) \
	$(SYSTEM)(do_endksig.o) \
	$(SYSTEM)(do_kill.o) \
	$(SYSTEM)(do_sigsend.o) \
	$(SYSTEM)(do_sigreturn.o) \
	$(SYSTEM)(do_abort.o) \
	$(SYSTEM)(do_getinfo.o) \
	$(SYSTEM)(do_sprofile.o) \
	$(SYSTEM)(do_cprofile.o) \
	$(SYSTEM)(do_profbuf.o) \
	$(SYSTEM)(do_vmctl.o)

build $(SYSTEM):	$(OBJECTS)
	aal cr $@ *.o

clean:
	rm -f $(SYSTEM) *.o *~ *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

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

$(SYSTEM)(do_runctl.o):	do_runctl.c
	$(CC) do_runctl.c

$(SYSTEM)(do_times.o):	do_times.c
	$(CC) do_times.c

$(SYSTEM)(do_setalarm.o):	do_setalarm.c
	$(CC) do_setalarm.c

$(SYSTEM)(do_stime.o):	do_stime.c
	$(CC) do_stime.c

$(SYSTEM)(do_vtimer.o):	do_vtimer.c
	$(CC) do_vtimer.c

$(SYSTEM)(do_irqctl.o):	do_irqctl.c
	$(CC) do_irqctl.c

$(SYSTEM)(do_devio.o):	do_devio.c
	$(CC) do_devio.c

$(SYSTEM)(do_vdevio.o):	do_vdevio.c
	$(CC) do_vdevio.c

$(SYSTEM)(do_copy.o):	do_copy.c
	$(CC) do_copy.c

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

$(SYSTEM)(do_setgrant.o):	do_setgrant.c
	$(CC) do_setgrant.c

$(SYSTEM)(do_privctl.o):	do_privctl.c
	$(CC) do_privctl.c

$(SYSTEM)(do_safecopy.o):	do_safecopy.c
	$(CC) do_safecopy.c

$(SYSTEM)(do_safemap.o):	do_safemap.c
	$(CC) do_safemap.c

$(SYSTEM)(do_sysctl.o):	do_sysctl.c
	$(CC) do_sysctl.c

$(SYSTEM)(do_segctl.o):	do_segctl.c
	$(CC) do_segctl.c

$(SYSTEM)(do_vm.o):	do_vm.o
do_vm.o:	do_vm.c
	$(CC) do_vm.c

$(SYSTEM)(do_sprofile.o):       do_sprofile.c
	$(CC) do_sprofile.c

$(SYSTEM)(do_cprofile.o):       do_cprofile.c
	$(CC) do_cprofile.c

$(SYSTEM)(do_profbuf.o):        do_profbuf.c
	$(CC) do_profbuf.c

$(SYSTEM)(do_vmctl.o):        do_vmctl.c
	$(CC) do_vmctl.c
