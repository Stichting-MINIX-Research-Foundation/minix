# Makefile for lib/syslib.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBSYS	= ../libsys.a
all:	$(LIBSYS)

clean: 
	rm *.o

OBJECTS	= \
	$(LIBSYS)(sys_times.o) \
	$(LIBSYS)(sys_abort.o) \
	$(LIBSYS)(sys_exec.o) \
	$(LIBSYS)(sys_fork.o) \
	$(LIBSYS)(sys_int86.o) \
	$(LIBSYS)(sys_kill.o) \
	$(LIBSYS)(sys_newmap.o) \
	$(LIBSYS)(sys_sigsend.o) \
	$(LIBSYS)(sys_sigreturn.o) \
	$(LIBSYS)(sys_endsig.o) \
	$(LIBSYS)(sys_getsig.o) \
	$(LIBSYS)(sys_svrctl.o) \
	$(LIBSYS)(sys_trace.o) \
	$(LIBSYS)(sys_exit.o) \
	$(LIBSYS)(sys_sdevio.o) \
	$(LIBSYS)(sys_getinfo.o) \
	$(LIBSYS)(sys_irqctl.o) \
	$(LIBSYS)(sys_segctl.o) \
	$(LIBSYS)(sys_nice.o) \
	$(LIBSYS)(sys_umap.o) \
	$(LIBSYS)(sys_physcopy.o) \
	$(LIBSYS)(sys_vircopy.o) \
	$(LIBSYS)(sys_in.o) \
	$(LIBSYS)(sys_out.o) \
	$(LIBSYS)(sys_vinb.o) \
	$(LIBSYS)(sys_vinw.o) \
	$(LIBSYS)(sys_vinl.o) \
	$(LIBSYS)(sys_voutb.o) \
	$(LIBSYS)(sys_voutw.o) \
	$(LIBSYS)(sys_voutl.o) \
	$(LIBSYS)(sys_setalarm.o) \
	$(LIBSYS)(sys_memset.o) \
	$(LIBSYS)(taskcall.o) \

$(LIBSYS):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBSYS)(sys_times.o):	sys_times.c
	$(CC1) sys_times.c

$(LIBSYS)(sys_getuptm.o): sys_getuptm.c
	$(CC1) sys_getuptm.c

$(LIBSYS)(sys_abort.o):	sys_abort.c
	$(CC1) sys_abort.c

$(LIBSYS)(sys_exec.o):	sys_exec.c
	$(CC1) sys_exec.c

$(LIBSYS)(sys_fork.o):	sys_fork.c
	$(CC1) sys_fork.c

$(LIBSYS)(sys_int86.o):	sys_int86.c
	$(CC1) sys_int86.c

$(LIBSYS)(sys_kill.o):	sys_kill.c
	$(CC1) sys_kill.c

$(LIBSYS)(sys_newmap.o):	sys_newmap.c
	$(CC1) sys_newmap.c

$(LIBSYS)(sys_svrctl.o):	sys_svrctl.c
	$(CC1) sys_svrctl.c

$(LIBSYS)(sys_trace.o):	sys_trace.c
	$(CC1) sys_trace.c

$(LIBSYS)(sys_exit.o):	sys_exit.c
	$(CC1) sys_exit.c

$(LIBSYS)(sys_sdevio.o):	sys_sdevio.c
	$(CC1) sys_sdevio.c

$(LIBSYS)(sys_getinfo.o):	sys_getinfo.c
	$(CC1) sys_getinfo.c

$(LIBSYS)(sys_irqctl.o):	sys_irqctl.c
	$(CC1) sys_irqctl.c

$(LIBSYS)(sys_eniop.o):	sys_eniop.c
	$(CC1) sys_eniop.c

$(LIBSYS)(sys_nice.o):	sys_nice.c
	$(CC1) sys_nice.c

$(LIBSYS)(sys_segctl.o):	sys_segctl.c
	$(CC1) sys_segctl.c

$(LIBSYS)(sys_umap.o):	sys_umap.c
	$(CC1) sys_umap.c

$(LIBSYS)(sys_getsig.o):	sys_getsig.c
	$(CC1) sys_getsig.c

$(LIBSYS)(sys_endsig.o):	sys_endsig.c
	$(CC1) sys_endsig.c

$(LIBSYS)(sys_sigsend.o):	sys_sigsend.c
	$(CC1) sys_sigsend.c

$(LIBSYS)(sys_sigreturn.o):	sys_sigreturn.c
	$(CC1) sys_sigreturn.c

$(LIBSYS)(sys_physcopy.o):	sys_physcopy.c
	$(CC1) sys_physcopy.c

$(LIBSYS)(sys_vircopy.o):	sys_vircopy.c
	$(CC1) sys_vircopy.c

$(LIBSYS)(sys_out.o):		sys_out.c
	$(CC1) sys_out.c

$(LIBSYS)(sys_in.o):		sys_in.c
	$(CC1) sys_in.c

$(LIBSYS)(sys_voutb.o):	sys_voutb.c
	$(CC1) sys_voutb.c

$(LIBSYS)(sys_voutw.o):	sys_voutw.c
	$(CC1) sys_voutw.c

$(LIBSYS)(sys_voutl.o):	sys_voutl.c
	$(CC1) sys_voutl.c

$(LIBSYS)(sys_vinb.o):	sys_vinb.c
	$(CC1) sys_vinb.c

$(LIBSYS)(sys_vinw.o):	sys_vinw.c
	$(CC1) sys_vinw.c

$(LIBSYS)(sys_vinl.o):	sys_vinl.c
	$(CC1) sys_vinl.c

$(LIBSYS)(sys_setalarm.o):	sys_setalarm.c
	$(CC1) sys_setalarm.c

$(LIBSYS)(sys_memset.o):	sys_memset.c
	$(CC1) sys_memset.c

$(LIBSYS)(taskcall.o):	taskcall.c
	$(CC1) taskcall.c

