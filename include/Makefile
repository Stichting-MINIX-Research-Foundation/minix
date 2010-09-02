# Doing a make includes builds /usr/include

INCS=	alloca.h ansi.h a.out.h ar.h assert.h configfile.h ctype.h \
	curses.h dirent.h env.h err.h errno.h fcntl.h fenv.h \
	float.h fnmatch.h fts.h getopt.h glob.h grp.h ifaddrs.h \
	inttypes.h libgen.h lib.h libutil.h limits.h locale.h \
	mathconst.h math.h midiparser.h netdb.h pwd.h regex.h \
	regexp.h setjmp.h sgtty.h signal.h stdarg.h stddef.h \
	stdint.h stdbool.h stdio.h stdlib.h string.h strings.h sysexits.h \
	syslog.h tar.h termcap.h termios.h time.h timers.h tools.h \
	ttyent.h ucontext.h unistd.h utime.h utmp.h wchar.h wctype.h \
	hgfs.h tzfile.h util.h fetch.h mpool.h ndbm.h db.h poll.h resolv.h
INCS+=	arpa/inet.h arpa/nameser.h
INCS+=	minix/a.out.h minix/bitmap.h minix/callnr.h minix/cdrom.h \
	minix/com.h minix/config.h minix/const.h minix/cpufeature.h \
	minix/crtso.h minix/debug.h minix/devio.h minix/dirent.h \
	minix/dir.h minix/dl_eth.h minix/dmap.h minix/driver.h \
	minix/limits.h \
	minix/acpi.h \
	minix/drivers.h minix/drvlib.h minix/ds.h minix/endpoint.h \
	minix/fslib.h minix/ioctl.h minix/ipc.h minix/ipcconst.h \
	minix/keymap.h minix/minlib.h minix/mq.h \
	minix/netdriver.h minix/partition.h minix/paths.h \
	minix/portio.h minix/priv.h minix/profile.h minix/queryparam.h \
	minix/rs.h minix/safecopies.h minix/sched.h minix/sef.h minix/sound.h \
	minix/spin.h minix/sys_config.h minix/sysinfo.h minix/syslib.h \
	minix/sysutil.h minix/timers.h minix/tty.h minix/type.h minix/types.h \
	minix/u64.h minix/vfsif.h minix/vm.h minix/vtreefs.h minix/gcov.h  \
	minix/compiler.h minix/compiler-ack.h minix/sha2.h minix/sha1.h minix/md5.h \
	minix/audio_fw.h
INCS+=	net/hton.h net/if.h net/ioctl.h net/netlib.h
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
INCS+=	netinet/if_ether.h netinet/in.h netinet/tcp.h

.include <bsd.own.mk>

INCSDIR=        /usr/include

SUBDIR=		sys arch

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
