.include <bsd.own.mk>

.PATH: ${MINIXSRCDIR}/common/include

INCS+= 	lib.h

INCS+= 	minix/ansi.h minix/bitmap.h minix/callnr.h minix/com.h \
	minix/compiler.h minix/config.h minix/const.h minix/debug.h \
	minix/devio.h minix/ds.h minix/endpoint.h minix/gcov.h \
	minix/ioctl.h minix/ipc.h minix/ipcconst.h minix/keymap.h \
	minix/limits.h minix/minlib.h minix/portio.h minix/priv.h \
	minix/profile.h minix/rs.h minix/safecopies.h minix/sef.h \
	minix/spin.h minix/sys_config.h minix/sysinfo.h minix/syslib.h \
	minix/sysutil.h minix/timers.h minix/type.h minix/u64.h minix/vm.h

INCS+=	net/gen/arp_io.h net/gen/dhcp.h net/gen/ether.h \
	net/gen/eth_hdr.h net/gen/eth_io.h net/gen/icmp.h \
	net/gen/icmp_hdr.h net/gen/if_ether.h net/gen/inet.h \
	net/gen/in.h net/gen/ip_hdr.h net/gen/ip_io.h \
	net/gen/nameser.h net/gen/netdb.h net/gen/oneCsum.h \
	net/gen/psip_hdr.h net/gen/psip_io.h net/gen/resolv.h \
	net/gen/rip.h net/gen/route.h net/gen/socket.h \
	net/gen/tcp.h net/gen/tcp_hdr.h net/gen/tcp_io.h \
	net/gen/udp.h net/gen/udp_hdr.h net/gen/udp_io.h \
	net/gen/vjhc.h
