# Makefile for system library implementation

# Directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
m = $i/ibm
l = $u/lib
n = $i/net
g = $n/gen
x = .
k = ..

# Programs, flags, etc.
CC =	exec cc
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -I$i
LDFLAGS = -i

SYS = 	clock.o copying.o debugging.o devio.o irqctl.o proctl.o \
	sysctl.o misc.o sigctl.o tracing.o \

# What to make.
all build: $(SYS)
	aal cr system.a $(SYS)

clean:
	rm -f *.a *.o *.bak 

# Dependencies from src/kernel/kernel.h
a =	$h/config.h $h/const.h $h/type.h $h/ipc.h \
	$i/string.h $i/limits.h $i/errno.h $i/stddef.h \
	$s/types.h \
	$m/portio.h \
	$k/proc.h $k/const.h $k/type.h $k/proto.h $k/glo.h 

# Dependencies from src/kernel/system.h
b = 	$k/system.h $h/com.h $k/proc.h 

clock.o:	$a $b 
copying.o:	$a $b 
debugging.o:	$a $b 
devio.o:	$a $b $h/devio.h 
irqctl.o:	$a $b 
misc.o: 	$a $b $i/unistd.h
proctl.o:	$a $b $k/sendmask.h $k/protect.h $i/signal.h  
sigctl.o:	$a $b $i/signal.h $s/sigcontext.h 
sysctl.o:	$a $b $s/svrctl.h $k/sendmask.h 
tracing.o:	$a $b $s/ptrace.h



