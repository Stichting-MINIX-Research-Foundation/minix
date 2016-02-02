#	$NetBSD: Makefile.inc,v 1.22 2015/04/15 19:13:47 mrg Exp $

# librpc sources
.PATH:	${.CURDIR}/rpc

SRCS+=	auth_none.c auth_unix.c authunix_prot.c bindresvport.c \
	clnt_bcast.c clnt_dg.c clnt_generic.c clnt_perror.c \
	clnt_raw.c clnt_simple.c \
	clnt_vc.c rpc_dtablesize.c \
	getnetconfig.c getnetpath.c getrpcent.c getrpcport.c \
	mt_misc.c pmap_clnt.c pmap_getmaps.c pmap_getport.c pmap_prot.c \
	pmap_prot2.c pmap_rmt.c rpc_prot.c rpc_commondata.c rpc_callmsg.c \
	rpc_generic.c rpc_soc.c rpcb_clnt.c rpcb_prot.c rpcb_st_xdr.c \
	svc.c svc_auth.c svc_dg.c svc_auth_unix.c svc_generic.c svc_raw.c \
	svc_run.c svc_simple.c svc_vc.c \
	xdr.c xdr_array.c xdr_float.c xdr_mem.c xdr_rec.c xdr_reference.c \
	xdr_stdio.c xdr_sizeof.c __rpc_getxid.c

CPPFLAGS+=	-DPORTMAP

MAN+=	bindresvport.3 getnetconfig.3 getnetpath.3 getrpcent.3 getrpcport.3 \
	rpc.3 rpc_soc.3 rpc_clnt_auth.3 rpc_clnt_calls.3 rpc_clnt_create.3 \
	rpc_svc_calls.3 rpc_svc_create.3 rpc_svc_err.3 rpc_svc_reg.3 \
	rpc_xdr.3 rpcbind.3 xdr.3
MLINKS+=	bindresvport.3 bindresvport_sa.3 \
		getnetconfig.3 setnetconfig.3 \
		getnetconfig.3 getnetconfigent.3 \
		getnetconfig.3 endnetconfig.3 \
		getnetconfig.3 nc_perror.3 \
		getnetconfig.3 nc_sperror.3 \
		getnetpath.3 setnetpath.3 \
		getnetpath.3 endnetpath.3 \
		getrpcent.3 getrpcbyname.3 \
		getrpcent.3 getrpcbynumber.3 \
		getrpcent.3 endrpcent.3 \
		getrpcent.3 setrpcent.3 \
		rpc_clnt_auth.3 auth_destroy.3 \
		rpc_clnt_auth.3 authnone_create.3 \
		rpc_clnt_auth.3 authsys_create.3 \
		rpc_clnt_auth.3 authsys_create_default.3 \
		rpc_clnt_calls.3 clnt_call.3 \
		rpc_clnt_calls.3 clnt_perrno.3 \
		rpc_clnt_calls.3 clnt_perror.3 \
		rpc_clnt_calls.3 clnt_sperrno.3 \
		rpc_clnt_calls.3 clnt_sperror.3 \
		rpc_clnt_calls.3 rpc_call.3 \
		rpc_clnt_calls.3 rpc_broadcast.3 \
		rpc_clnt_calls.3 rpc_broadcast_exp.3 \
		rpc_clnt_calls.3 clnt_freeres.3 \
		rpc_clnt_calls.3 clnt_geterr.3 \
		rpc_clnt_create.3 clnt_control.3 \
		rpc_clnt_create.3 clnt_create.3 \
		rpc_clnt_create.3 clnt_create_vers.3 \
		rpc_clnt_create.3 clnt_destroy.3 \
		rpc_clnt_create.3 clnt_pcreateerror.3 \
		rpc_clnt_create.3 clnt_spcreateerror.3 \
		rpc_clnt_create.3 clnt_dg_create.3 \
		rpc_clnt_create.3 clnt_raw_create.3 \
		rpc_clnt_create.3 clnt_tli_create.3 \
		rpc_clnt_create.3 clnt_tp_create.3 \
		rpc_clnt_create.3 clnt_vc_create.3 \
		rpc_svc_calls.3 svc_dg_enablecache.3 \
		rpc_svc_calls.3 svc_exit.3 \
		rpc_svc_calls.3 svc_freeargs.3 \
		rpc_svc_calls.3 svc_getargs.3 \
		rpc_svc_calls.3 svc_getreq_common.3 \
		rpc_svc_calls.3 svc_getreq_poll.3 \
		rpc_svc_calls.3 svc_getreqset.3 \
		rpc_svc_calls.3 svc_getrpccaller.3 \
		rpc_svc_calls.3 __svc_getcallercreds.3 \
		rpc_svc_calls.3 svc_pollset.3 \
		rpc_svc_calls.3 svc_run.3 \
		rpc_svc_calls.3 svc_sendreply.3 \
		rpc_svc_create.3 svc_control.3 \
		rpc_svc_create.3 svc_create.3 \
		rpc_svc_create.3 svc_dg_create.3 \
		rpc_svc_create.3 svc_destroy.3 \
		rpc_svc_create.3 svc_fd_create.3 \
		rpc_svc_create.3 svc_raw_create.3 \
		rpc_svc_create.3 svc_tli_create.3 \
		rpc_svc_create.3 svc_tp_create.3 \
		rpc_svc_create.3 svc_vc_create.3 \
		rpc_svc_err.3 svcerr_auth.3 \
		rpc_svc_err.3 svcerr_decode.3 \
		rpc_svc_err.3 svcerr_noproc.3 \
		rpc_svc_err.3 svcerr_noprog.3 \
		rpc_svc_err.3 svcerr_progvers.3 \
		rpc_svc_err.3 svcerr_systemerr.3 \
		rpc_svc_err.3 svcerr_weakauth.3 \
		rpc_svc_reg.3 rpc_reg.3 \
		rpc_svc_reg.3 svc_reg.3 \
		rpc_svc_reg.3 svc_unreg.3 \
		rpc_svc_reg.3 svc_auth_reg.3 \
		rpc_svc_reg.3 xprt_register.3 \
		rpc_svc_reg.3 xprt_unregister.3 \
		rpc_xdr.3 xdr_accepted_reply.3 \
		rpc_xdr.3 xdr_authsys_parms.3 \
		rpc_xdr.3 xdr_callhdr.3 \
		rpc_xdr.3 xdr_callmsg.3 \
		rpc_xdr.3 xdr_opaque_auth.3 \
		rpc_xdr.3 xdr_rejected_reply.3 \
		rpc_xdr.3 xdr_replymsg.3 \
		rpcbind.3 rpcb_getmaps.3 \
		rpcbind.3 rpcb_getaddr.3 \
		rpcbind.3 rpcb_gettime.3 \
		rpcbind.3 rpcb_rmtcall.3 \
		rpcbind.3 rpcb_set.3 \
		rpcbind.3 rpcb_unset.3 \
		rpc_soc.3 authunix_create.3 \
		rpc_soc.3 authunix_create_default.3 \
		rpc_soc.3 callrpc.3 \
		rpc_soc.3 clnt_broadcast.3 \
		rpc_soc.3 clntraw_create.3 \
		rpc_soc.3 clnttcp_create.3 \
		rpc_soc.3 clntudp_bufcreate.3 \
		rpc_soc.3 clntudp_create.3 \
		rpc_soc.3 get_myaddress.3 \
		rpc_soc.3 pmap_getmaps.3 \
		rpc_soc.3 pmap_getport.3 \
		rpc_soc.3 pmap_rmtcall.3 \
		rpc_soc.3 pmap_set.3 \
		rpc_soc.3 pmap_unset.3 \
		rpc_soc.3 registerrpc.3 \
		rpc_soc.3 rpc_createerr.3 \
		rpc_soc.3 svc_fds.3 \
		rpc_soc.3 svc_fdset.3 \
		rpc_soc.3 svc_getcaller.3 \
		rpc_soc.3 svc_register.3 \
		rpc_soc.3 svc_unregister.3 \
		rpc_soc.3 svcfd_create.3 \
		rpc_soc.3 svcraw_create.3 \
		rpc_soc.3 svctcp_create.3 \
		rpc_soc.3 svcudp_create.3 \
		rpc_soc.3 svcudp_bufcreate.3 \
		rpc_soc.3 xdr_pmap.3 \
		rpc_soc.3 xdr_pmaplist.3 \
		xdr.3 xdr_array.3 \
		xdr.3 xdr_bool.3 \
		xdr.3 xdr_bytes.3 \
		xdr.3 xdr_char.3 \
		xdr.3 xdr_destroy.3 \
		xdr.3 xdr_double.3 \
		xdr.3 xdr_enum.3 \
		xdr.3 xdr_float.3 \
		xdr.3 xdr_free.3 \
		xdr.3 xdr_getpos.3 \
		xdr.3 xdr_inline.3 \
		xdr.3 xdr_int.3 \
		xdr.3 xdr_long.3 \
		xdr.3 xdrmem_create.3 \
		xdr.3 xdr_opaque.3 \
		xdr.3 xdr_pointer.3 \
		xdr.3 xdrrec_create.3 \
		xdr.3 xdrrec_endofrecord.3 \
		xdr.3 xdrrec_eof.3 \
		xdr.3 xdrrec_skiprecord.3 \
		xdr.3 xdr_reference.3 \
		xdr.3 xdr_setpos.3 \
		xdr.3 xdr_short.3 \
		xdr.3 xdr_sizeof.3 \
		xdr.3 xdrstdio_create.3 \
		xdr.3 xdr_string.3 \
		xdr.3 xdr_u_char.3 \
		xdr.3 xdr_u_long.3 \
		xdr.3 xdr_u_short.3 \
		xdr.3 xdr_union.3 \
		xdr.3 xdr_vector.3 \
		xdr.3 xdr_void.3 \
		xdr.3 xdr_wrapstring.3
