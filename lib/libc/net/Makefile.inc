#	$NetBSD: Makefile.inc,v 1.85 2013/10/19 19:56:56 christos Exp $
#	@(#)Makefile.inc	8.2 (Berkeley) 9/5/93

# net sources
.if defined(__MINIX)
.PATH: ${NETBSDSRCDIR}/minix/lib/libc/net

CPPFLAGS.getpeereid.c+= -D_MINIX_SYSTEM=1
CPPFLAGS.getsockopt.c+= -D_MINIX_SYSTEM=1
CPPFLAGS.setsockopt.c+= -D_MINIX_SYSTEM=1
.endif
.PATH: ${ARCHDIR}/net ${.CURDIR}/net

# Not supported by minix: iso_addr.c link_addr.c sockatmark.c
SRCS+=	base64.c ethers.c gethnamaddr.c getifaddrs.c \
	getnetnamadr.c getnetent.c getpeereid.c \
	getprotobyname.c getprotobynumber.c getprotoent.c \
	getprotobyname_r.c getprotobynumber_r.c getprotoent_r.c \
	getservbyname.c getservbyport.c getservent.c \
	getservbyname_r.c getservbyport_r.c getservent_r.c \
	nsdispatch.c nslexer.l nsparser.y nsap_addr.c \
	rcmd.c recv.c send.c sethostent.c \
#	sockatmark.c

.if (${MKHESIOD} != "no")
SRCS+=	hesiod.c
.if ${HAVE_GCC:U} == "45" && ${MACHINE_CPU} == "ia64"
COPTS.hesiod.c+=	${${ACTIVE_CC} == "gcc" :? -O1 :}
.endif
.endif

SRCS+=	getaddrinfo.c getnameinfo.c
.if (${USE_INET6} != "no")
SRCS+=	ip6opt.c rthdr.c vars6.c inet6_scopeid.c
.endif
.if !defined(__MINIX)
SRCS+=	if_indextoname.c if_nameindex.c if_nametoindex.c
.endif

LPREFIX=_nsyy
YPREFIX=_nsyy
YHEADER=1

CLEANFILES+=nsparser.c nslexer.c nsparser.h

nslexer.c: nslexer.l nsparser.h
	${_MKTARGET_LEX}
	${LEX.l} -t ${.IMPSRC} | sed -e '/YY_BUF_SIZE/s/16384/1024/' >${.TARGET}

# machine-dependent net sources
# m-d Makefile.inc must include sources for:
#	htonl() htons() ntohl() ntohs()

.include "${ARCHDIR}/net/Makefile.inc"

.if ${HAVE_GCC:U} == "45" && ${MACHINE_CPU} == "arm"
COPTS.getaddrinfo.c+=	${${ACTIVE_CC} == "gcc" :? -fno-tree-ter :}
COPTS.gethnamaddr.c+=	${${ACTIVE_CC} == "gcc" :? -fno-tree-ter :}
.endif

.if defined(__MINIX)
COPTS.gethnamaddr.c+=	${${CC} == "gcc" :? -std=c99 :}
.endif # defined(__MINIX)

MAN+=	byteorder.3 ethers.3 gethostbyname.3 getifaddrs.3 \
	getnetent.3 getprotoent.3 getpeereid.3 \
	getservent.3 inet.3 inet_net.3 linkaddr.3 \
	nsdispatch.3 rcmd.3 resolver.3 sockatmark.3

.if (${MKHESIOD} != "no")
MAN+=	hesiod.3
.endif

MLINKS+=byteorder.3 htonl.3 byteorder.3 htons.3 byteorder.3 ntohl.3 \
	byteorder.3 ntohs.3
MLINKS+=ethers.3 ether_aton.3 ethers.3 ether_hostton.3 ethers.3 ether_line.3 \
	ethers.3 ether_ntoa.3 ethers.3 ether_ntohost.3
MLINKS+=gethostbyname.3 endhostent.3 gethostbyname.3 gethostbyaddr.3 \
	gethostbyname.3 gethostent.3 gethostbyname.3 sethostent.3 \
	gethostbyname.3 herror.3 gethostbyname.3 hstrerror.3 \
	gethostbyname.3 gethostbyname2.3
MLINKS+=getnetent.3 endnetent.3 getnetent.3 getnetbyaddr.3 \
	getnetent.3 getnetbyname.3 getnetent.3 setnetent.3
MLINKS+=getprotoent.3 endprotoent.3 getprotoent.3 getprotobyname.3 \
	getprotoent.3 getprotobynumber.3 getprotoent.3 setprotoent.3
MLINKS+=getservent.3 endservent.3 getservent.3 getservbyname.3 \
	getservent.3 getservbyport.3 getservent.3 setservent.3
.if (${MKHESIOD} != "no")
MLINKS+=hesiod.3 hesiod_end.3 hesiod.3 hesiod_free_list.3 \
	hesiod.3 hesiod_init.3 hesiod.3 hesiod_resolve.3 \
	hesiod.3 hesiod_to_bind.3
.endif
MLINKS+=inet.3 addr.3 inet.3 inet_addr.3 inet.3 inet_aton.3 \
	inet.3 inet_lnaof.3 inet.3 inet_makeaddr.3 inet.3 inet_netof.3 \
	inet.3 inet_network.3 inet.3 inet_ntoa.3 inet.3 network.3 \
	inet.3 ntoa.3 inet.3 inet_ntop.3 inet.3 inet_pton.3
MLINKS+=inet_net.3 inet_net_ntop.3 inet_net.3 inet_net_pton.3
MLINKS+=linkaddr.3 link_addr.3 linkaddr.3 link_ntoa.3 linkaddr.3 linkntoa.3
MLINKS+=rcmd.3 iruserok.3 rcmd.3 rresvport.3 rcmd.3 ruserok.3 rcmd.3 orcmd.3
MLINKS+=resolver.3 dn_comp.3 \
	resolver.3 dn_expand.3 \
	resolver.3 fp_nquery.3 \
	resolver.3 fp_resstat.3 \
	resolver.3 hostalias.3 \
	resolver.3 p_query.3 \
	resolver.3 res_close.3 \
	resolver.3 res_findzonecut.3 \
	resolver.3 res_getservers.3 \
	resolver.3 res_hostalias.3 \
	resolver.3 res_init.3 \
	resolver.3 res_isourserver.3 \
	resolver.3 res_mkquery.3 \
	resolver.3 res_nclose.3 \
	resolver.3 res_ndestroy.3 \
	resolver.3 res_ninit.3 \
	resolver.3 res_nmkquery.3 \
	resolver.3 res_nmkupdate.3 \
	resolver.3 res_nquery.3 \
	resolver.3 res_nquerydomain.3 \
	resolver.3 res_nsearch.3 \
	resolver.3 res_nsend.3 \
	resolver.3 res_nsendsigned.3 \
	resolver.3 res_nupdate.3 \
	resolver.3 res_ourserver_p.3 \
	resolver.3 res_pquery.3 \
	resolver.3 res_query.3 \
	resolver.3 res_querydomain.3 \
	resolver.3 res_search.3 \
	resolver.3 res_send.3 \
	resolver.3 res_setservers.3 \
	resolver.3 res_update.3

# IPv6
MAN+=	gai_strerror.3 getaddrinfo.3 getnameinfo.3 if_indextoname.3 \
	inet6_option_space.3 inet6_rthdr_space.3 \
	inet6_opt_init.3 inet6_rth_space.3 inet6_getscopeid.3
MLINKS+=getaddrinfo.3 freeaddrinfo.3 \
	getaddrinfo.3 allocaddrinfo.3 \
	getifaddrs.3 freeifaddrs.3 \
	if_indextoname.3 if_nametoindex.3 if_indextoname.3 if_nameindex.3 \
	if_indextoname.3 if_freenameindex.3 \
	inet6_getscopeid.3 inet6_putscopeid.3 \
	inet6_option_space.3 inet6_option_init.3 \
	inet6_option_space.3 inet6_option_append.3 \
	inet6_option_space.3 inet6_option_alloc.3 \
	inet6_option_space.3 inet6_option_next.3 \
	inet6_option_space.3 inet6_option_find.3 \
	inet6_rthdr_space.3 inet6_rthdr_init.3 \
	inet6_rthdr_space.3 inet6_rthdr_add.3 \
	inet6_rthdr_space.3 inet6_rthdr_lasthop.3 \
	inet6_rthdr_space.3 inet6_rthdr_reverse.3 \
	inet6_rthdr_space.3 inet6_rthdr_segments.3 \
	inet6_rthdr_space.3 inet6_rthdr_getaddr.3 \
	inet6_rthdr_space.3 inet6_rthdr_getflags.3 \
	inet6_opt_init.3 inet6_opt_append.3 \
	inet6_opt_init.3 inet6_opt_finish.3 \
	inet6_opt_init.3 inet6_opt_set_val.3 \
	inet6_opt_init.3 inet6_opt_next.3 \
	inet6_opt_init.3 inet6_opt_find.3 \
	inet6_opt_init.3 inet6_opt_get_val.3 \
	inet6_rth_space.3 inet6_rth_init.3 \
	inet6_rth_space.3 inet6_rth_add.3 \
	inet6_rth_space.3 inet6_rth_reverse.3 \
	inet6_rth_space.3 inet6_rth_segments.3 \
	inet6_rth_space.3 inet6_rth_getaddr.3 \
	rcmd.3 rcmd_af.3 rcmd.3 iruserok_sa.3 rcmd.3 rresvport_af.3 \
	rcmd.3 orcmd_af.3
