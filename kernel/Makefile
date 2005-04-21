# Makefile for kernel

# Directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
b = $i/ibm
l = $u/lib
n = $i/net
g = $n/gen

# Programs, flags, etc.
CC =	exec cc
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -I$i
LDFLAGS = -i 
LIBS = -lsys -lutils -ltimers

HEAD =	mpx.o

OBJS =	start.o protect.o klibc.o klib.o table.o main.o proc.o \
	i8259.o exception.o system.o clock.o memory.o misc.o \
	dummy.o \
	rtl8139.o pci.o pci_table.o  

SYS =	system/system.a 


# What to make.
kernel build: $(HEAD) $(OBJS) $(SYS) 
	$(LD) $(LDFLAGS) -o $@ $(HEAD) $(OBJS) $(SYS) $(CLOCK) $(LIBS)
	install -S 0 $@

$(SYS):
	cd system && $(MAKE)

all install:
	cd system && $(MAKE) -$(MAKEFLAGS) $@

clean:
	cd system && $(MAKE) -$(MAKEFLAGS) $@
	rm -f *.o *.bak kernel

# Dependencies
a =	kernel.h const.h type.h proto.h glo.h \
	$h/config.h $h/const.h $h/type.h $h/ipc.h \
	$s/types.h \
	$i/string.h $i/limits.h $i/errno.h $i/timers.h \
	$b/portio.h $b/interrupt.h $b/bios.h $b/ports.h

klibc.o: $a

klib.o: $h/config.h $h/const.h const.h sconst.h protect.h
klib.o: klib88.s klib386.s

mpx.o: $h/config.h $h/const.h $h/com.h const.h protect.h sconst.h
mpx.o: mpx88.s mpx386.s
mpx.o: mpx88.s mpx386.s

clock.o:	$a
clock.o:	$i/signal.h
clock.o:	$h/callnr.h
clock.o:	$h/com.h
clock.o:	proc.h

start.o:	$a
start.o:	$i/stdlib.h
start.o:	protect.h

exception.o:	$a
exception.o:	$i/signal.h
exception.o:	$h/com.h
exception.o:	proc.h

driver.o:	$a $d
driver.o:	$h/ioctl.h
driver.o:	$s/ioc_disk.h

drvlib.o:	$a $d $(dl)

i8259.o:	$a

main.o:	$a
main.o:	$i/unistd.h
main.o:	$i/signal.h
main.o:	$i/a.out.h
main.o:	$h/callnr.h
main.o:	$h/com.h
main.o:	proc.h
main.o:	sendmask.h

memory.o:	$a
memory.o:	proc.h
memory.o:	protect.h

misc.o:	$a
misc.o:	$i/stdlib.h
misc.o:	$h/com.h
misc.o:	assert.h

printer.o:	$a
printer.o:	$h/callnr.h
printer.o:	$h/com.h
printer.o:	proc.h

proc.o:	$a
proc.o:	$h/callnr.h
proc.o:	$h/com.h
proc.o:	proc.h
proc.o:	sendmask.h

protect.o:	$a
protect.o:	$h/com.h
protect.o:	proc.h
protect.o:	protect.h

system.o:	$a
system.o:	$i/stdlib.h
system.o:	$i/signal.h
system.o:	$i/unistd.h
system.o:	$s/sigcontext.h
system.o:	$s/ptrace.h
system.o:	$h/ioctl.h
system.o:	$s/svrctl.h
system.o:	$h/callnr.h
system.o:	$h/com.h
system.o:	system.h
system.o:	proc.h
system.o:	protect.h
system.o:	assert.h
system.o:	sendmask.h

table.o:	$a
table.o:	$i/stdlib.h
table.o:	$i/termios.h
table.o:	$h/com.h
table.o:	proc.h
table.o:	sendmask.h
table.o:	$b/int86.h

rtl8139.o:	$a
rtl8139.o:	$h/com.h
rtl8139.o:	$n/hton.h
rtl8139.o:	$g/ether.h
rtl8139.o:	$g/eth_io.h
rtl8139.o:	$i/stddef.h
rtl8139.o:	assert.h
rtl8139.o:	pci.h
rtl8139.o:	proc.h
rtl8139.o:	rtl8139.h

pci.o:		$a
pci.o:		assert.h
pci.o:		pci.h
pci.o:		pci_amd.h
pci.o:		pci_intel.h
pci.o:		pci_via.h
pci.o:		pci_sis.h

pci_table.o:	$a
pci_table.o:	pci.h

dummy.o:	$a

system/system.a:	$a $h/devio.h $h/com.h
system/system.a:	proc.h protect.h system.h sendmask.h 
system/system.a:	$s/ptrace.h $s/sigcontext.h
system/system.a:	$i/signal.h $i/unistd.h
system/system.a:	system/alarms.c
system/system.a:	system/copying.c
system/system.a:	system/devio.c
system/system.a:	system/irqctl.c
system/system.a:	system/misc.c
system/system.a:	system/proctl.c
system/system.a:	system/sigctl.c
system/system.a:	system/srvrctl.c
system/system.a:	system/tracing.c
system/system.a:	system/debugging.c
system/system.a:	system/do_copy.c
system/system.a:	system/do_vcopy.c

