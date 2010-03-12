# Makefile for system library implementation

include /etc/make.conf

# Directories
u = /usr
i = $u/include
a = ../arch

# Programs, flags, etc.
#CC =	exec cc $(CFLAGS) -c
CC =	exec cc
#CCNOPROF =      exec cc $(CFLAGSNOPROF) -c      # no call profiling for these
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -I$i $(CPROFILE) -I$a/$(ARCH)/include
#CFLAGSNOPROF = -I$i
LDFLAGS = -i

SYSTEM	= ../system.a

# What to make.
all build install: $(SYSTEM)

OBJECTS	= \
	do_unused.o \
	do_fork.o \
	do_exec.o \
	do_newmap.o \
	do_exit.o \
	do_trace.o \
	do_nice.o \
	do_runctl.o \
	do_times.o \
	do_setalarm.o \
	do_stime.o \
	do_vtimer.o \
	do_irqctl.o \
	do_devio.o \
	do_vdevio.o \
	do_copy.o \
	do_umap.o \
	do_memset.o \
	do_setgrant.o \
	do_privctl.o \
	do_segctl.o \
	do_safecopy.o \
	do_safemap.o \
	do_sysctl.o \
	do_getksig.o \
	do_endksig.o \
	do_kill.o \
	do_sigsend.o \
	do_sigreturn.o \
	do_abort.o \
	do_getinfo.o \
	do_sprofile.o \
	do_cprofile.o \
	do_profbuf.o \
	do_vmctl.o \
	do_mcontext.o

build $(SYSTEM):	$(SYSTEM)($(OBJECTS))
	aal cr $@ *.o
#	aal cr $@ $(OBJECTS)
#	ar cru $(.TARGET) $(.OODATE)

clean:
	rm -f $(SYSTEM) *.o *~ *.bak 

depend: 
	mkdep "$(CC) $(CFLAGS) -E" *.c > .depend
