.include <bsd.own.mk>

.PATH: ${MINIXSRCDIR}/common/include

INCS+=  env.h fetch.h hgfs.h lib.h libutil.h timers.h

INCS+= 	minix/acpi.h minix/ansi.h minix/audio_fw.h minix/bitmap.h \
	minix/callnr.h minix/com.h minix/compiler.h minix/config.h \
	minix/const.h minix/cpufeature.h minix/crtso.h minix/debug.h \
	minix/devio.h minix/devman.h minix/dmap.h \
	minix/driver.h minix/drivers.h minix/drvlib.h minix/ds.h \
	minix/endpoint.h minix/fslib.h minix/gcov.h minix/hash.h \
	minix/ioctl.h minix/input.h minix/ipc.h minix/ipcconst.h \
	minix/keymap.h minix/limits.h minix/mthread.h minix/minlib.h \
	minix/mq.h minix/netdriver.h minix/partition.h minix/portio.h \
	minix/priv.h minix/procfs.h minix/profile.h minix/queryparam.h \
	minix/rs.h minix/safecopies.h minix/sched.h minix/sef.h \
	minix/sound.h minix/spin.h minix/sys_config.h minix/sysinfo.h \
	minix/syslib.h minix/sysutil.h minix/timers.h minix/type.h \
	minix/tty.h minix/u64.h minix/usb.h minix/usb_ch9.h minix/vm.h \
	minix/vfsif.h minix/vtreefs.h minix/libminixfs.h

INCS+=	net/gen/arp_io.h net/gen/dhcp.h net/gen/ether.h \
	net/gen/eth_hdr.h net/gen/eth_io.h net/gen/icmp.h \
	net/gen/icmp_hdr.h net/gen/if_ether.h net/gen/inet.h \
	net/gen/in.h net/gen/ip_hdr.h net/gen/ip_io.h \
	net/gen/oneCsum.h net/gen/psip_hdr.h net/gen/psip_io.h \
	net/gen/rip.h net/gen/route.h net/gen/socket.h \
	net/gen/tcp.h net/gen/tcp_hdr.h net/gen/tcp_io.h \
	net/gen/udp.h net/gen/udp_hdr.h net/gen/udp_io.h \
	net/gen/udp_io_hdr.h net/gen/vjhc.h

INCS+=	ddekit/assert.h ddekit/initcall.h ddekit/memory.h  ddekit/pgtab.h \
        ddekit/thread.h ddekit/attribs.h ddekit/inline.h  ddekit/printf.h \
        ddekit/timer.h ddekit/condvar.h ddekit/interrupt.h ddekit/panic.h \
        ddekit/resources.h  ddekit/types.h  ddekit/debug.h  ddekit/lock.h \
        ddekit/pci.h ddekit/semaphore.h  ddekit/usb.h  ddekit/minix/pci.h \
        ddekit/minix/msg_queue.h ddekit/ddekit.h
