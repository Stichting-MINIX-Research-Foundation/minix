# $NetBSD: Makefile,v 1.12 2014/11/19 19:33:31 christos Exp $	

USE_SHLIBDIR=yes
.include <bsd.own.mk>
USE_FORT?= yes	# network protocol library

CWARNFLAGS.clang+=	-Wno-format-extra-args \
			-Wno-unneeded-internal-declaration

CPPFLAGS+= -DPCAP_DONT_INCLUDE_PCAP_BPF_H -DHAVE_CONFIG_H

.SUFFIXES: .3 .3pcap

__sed: .USE
	@rm -f ${.TARGET}
	${TOOL_SED} -e 's,@MAN_FILE_FORMATS@,5,g' \
	    -e 's,@MAN_MISC_INFO@,7,g' \
	    -e 's,3PCAP,3,g' < ${.ALLSRC} > ${.TARGET}

.3pcap.3:
	@rm -f ${.TARGET}
	${TOOL_SED} -e 's,@MAN_FILE_FORMATS@,5,g' \
	    -e 's,@MAN_MISC_INFO@,7,g' \
	    -e 's,3PCAP,3,g' < ${.ALLSRC} > ${.TARGET}


SRCDIR=		${.CURDIR}/../dist
.PATH:		${SRCDIR}

LIB=		pcap
MAN3= \
pcap.3pcap \
pcap_activate.3pcap \
pcap_breakloop.3pcap \
pcap_can_set_rfmon.3pcap \
pcap_close.3pcap \
pcap_compile.3pcap \
pcap_create.3pcap \
pcap_datalink.3pcap \
pcap_datalink_name_to_val.3pcap \
pcap_datalink_val_to_name.3pcap \
pcap_dump.3pcap \
pcap_dump_close.3pcap \
pcap_dump_file.3pcap \
pcap_dump_flush.3pcap \
pcap_dump_ftell.3pcap \
pcap_dump_open.3pcap \
pcap_file.3pcap \
pcap_fileno.3pcap \
pcap_findalldevs.3pcap \
pcap_freecode.3pcap \
pcap_get_selectable_fd.3pcap \
pcap_geterr.3pcap \
pcap_inject.3pcap \
pcap_is_swapped.3pcap \
pcap_lib_version.3pcap \
pcap_list_datalinks.3pcap \
pcap_list_tstamp_types.3pcap \
pcap_lookupdev.3pcap \
pcap_lookupnet.3pcap \
pcap_loop.3pcap \
pcap_major_version.3pcap \
pcap_next_ex.3pcap \
pcap_offline_filter.3pcap \
pcap_open_dead.3pcap \
pcap_open_live.3pcap \
pcap_open_offline.3pcap \
pcap_set_buffer_size.3pcap \
pcap_set_datalink.3pcap \
pcap_set_immediate_mode.3pcap \
pcap_set_promisc.3pcap \
pcap_set_rfmon.3pcap \
pcap_set_snaplen.3pcap \
pcap_set_timeout.3pcap \
pcap_set_tstamp_type.3pcap \
pcap_setdirection.3pcap \
pcap_setfilter.3pcap \
pcap_setnonblock.3pcap \
pcap_snapshot.3pcap \
pcap_stats.3pcap \
pcap_statustostr.3pcap \
pcap_strerror.3pcap \
pcap_tstamp_type_name_to_val.3pcap \
pcap_tstamp_type_val_to_name.3pcap


MAN3IN= \
pcap.3pcap.in \
pcap_compile.3pcap.in \
pcap_datalink.3pcap.in \
pcap_dump_open.3pcap.in \
pcap_get_tstamp_precision.3pcap.in \
pcap_list_datalinks.3pcap.in \
pcap_list_tstamp_types.3pcap.in \
pcap_open_dead.3pcap.in \
pcap_open_offline.3pcap.in \
pcap_set_tstamp_precision.3pcap.in \
pcap_set_tstamp_type.3pcap.in

MANMISCIN= \
pcap-filter.manmisc.in \
pcap-linktype.manmisc.in \
pcap-tstamp.manmisc.in
MANFILEIN= pcap-savefile.manfile.in
CLEANFILES+= ${MAN}

MAN=	${MAN3:S/3pcap/3/g} ${MAN3IN:S/3pcap.in/3/g} \
	${MANMISCIN:S/manmisc.in/7/g} ${MANFILEIN:S/manfile.in/5/g}
WARNS?=		4

.for i in ${MAN3IN}
${i:S/pcap.in//}: ${i} __sed
.endfor

.for i in ${MANMISCIN}
${i:S/manmisc.in/7/}: ${i} __sed
.endfor

.for i in ${MANFILEIN}
${i:S/manfile.in/5/}: ${i} __sed
.endfor

CPPFLAGS+=	-I${.CURDIR}/../include -I${.CURDIR} -I${SRCDIR}
CPPFLAGS+=	-DYYBISON
LPREFIX=	pcap_
YPREFIX=	pcap_
YHEADER=	1

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-D_U_="__attribute__((__unused__))"

.if (${USE_INET6} != "no")
CPPFLAGS+=	-DINET6
.endif

SRCS=	scanner.l savefile.c pcap.c pcap-bpf.c optimize.c nametoaddr.c \
	inet.c grammar.y gencode.c fad-getad.c etherent.c bpf_image.c \
	bpf_dump.c version.c sf-pcap-ng.c sf-pcap.c pcap-common.c

.PATH:	${NETBSDSRCDIR}/sys/net
SRCS+=	bpf_filter.c

INCS=		pcap/pcap.h pcap/namedb.h
INCSDIR=	/usr/include
INCSYMLINKS+=   ../net/bpf.h ${INCSDIR}/pcap/bpf.h
INCSYMLINKS+=	pcap/pcap.h ${INCSDIR}/pcap.h
INCSYMLINKS+=	pcap/namedb.h ${INCSDIR}/pcap-namedb.h

scanner.d scanner.o: tokdefs.h

tokdefs.h: grammar.h
	cp $? $@

CLEANFILES+=	tokdefs.h
COPTS.bpf_image.c = -Wno-format-nonliteral

.include <bsd.lib.mk>
