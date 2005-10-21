# Makefile for lib/syslib.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE

LIBRARIES = libsys

libsys_OBJECTS	= \
	sys_times.o \
	sys_abort.o \
	sys_exec.o \
	sys_fork.o \
	sys_int86.o \
	sys_kill.o \
	sys_newmap.o \
	sys_sigsend.o \
	sys_sigreturn.o \
	sys_endsig.o \
	sys_getsig.o \
	sys_svrctl.o \
	sys_trace.o \
	sys_exit.o \
	sys_sdevio.o \
	sys_getinfo.o \
	sys_irqctl.o \
	sys_segctl.o \
	sys_nice.o \
	sys_umap.o \
	sys_physcopy.o \
	sys_vircopy.o \
	sys_in.o \
	sys_out.o \
	sys_vinb.o \
	sys_vinw.o \
	sys_vinl.o \
	sys_voutb.o \
	sys_voutw.o \
	sys_voutl.o \
	sys_setalarm.o \
	sys_memset.o \
	taskcall.o

include ../Makefile.inc
