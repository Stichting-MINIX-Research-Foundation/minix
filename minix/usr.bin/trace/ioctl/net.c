
#include "inc.h"

#include <sys/ioctl.h>
#include <sys/ucred.h>
#include <net/gen/in.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <net/gen/arp_io.h>
#include <net/gen/ip_io.h>
#include <net/gen/route.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <net/gen/udp_io_hdr.h>
#include <net/gen/psip_io.h>
#include <arpa/inet.h>

const char *
net_ioctl_name(unsigned long req)
{

	switch (req) {
	NAME(FIONREAD);
	NAME(NWIOSETHOPT);	/* TODO: print argument */
	NAME(NWIOGETHOPT);	/* TODO: print argument */
	NAME(NWIOGETHSTAT);	/* TODO: print argument */
	NAME(NWIOARPGIP);	/* TODO: print argument */
	NAME(NWIOARPGNEXT);	/* TODO: print argument */
	NAME(NWIOARPSIP);	/* TODO: print argument */
	NAME(NWIOARPDIP);	/* TODO: print argument */
	NAME(NWIOSIPCONF2);	/* TODO: print argument */
	NAME(NWIOSIPCONF);	/* TODO: print argument */
	NAME(NWIOGIPCONF2);	/* TODO: print argument */
	NAME(NWIOGIPCONF);	/* TODO: print argument */
	NAME(NWIOSIPOPT);
	NAME(NWIOGIPOPT);
	NAME(NWIOGIPOROUTE);	/* TODO: print argument */
	NAME(NWIOSIPOROUTE);	/* TODO: print argument */
	NAME(NWIODIPOROUTE);	/* TODO: print argument */
	NAME(NWIOGIPIROUTE);	/* TODO: print argument */
	NAME(NWIOSIPIROUTE);	/* TODO: print argument */
	NAME(NWIODIPIROUTE);	/* TODO: print argument */
	NAME(NWIOSTCPCONF);
	NAME(NWIOGTCPCONF);
	NAME(NWIOTCPCONN);
	NAME(NWIOTCPLISTEN);
	NAME(NWIOTCPATTACH);	/* TODO: print argument */
	NAME(NWIOTCPSHUTDOWN);	/* no argument */
	NAME(NWIOSTCPOPT);
	NAME(NWIOGTCPOPT);
	NAME(NWIOTCPPUSH);	/* no argument */
	NAME(NWIOTCPLISTENQ);
	NAME(NWIOGTCPCOOKIE);
	NAME(NWIOTCPACCEPTTO);
	NAME(NWIOTCPGERROR);
	NAME(NWIOSUDPOPT);
	NAME(NWIOGUDPOPT);
	NAME(NWIOUDPPEEK);	/* TODO: print argument */
	NAME(NWIOSPSIPOPT);	/* TODO: print argument */
	NAME(NWIOGPSIPOPT);	/* TODO: print argument */
	NAME(NWIOGUDSFADDR);
	NAME(NWIOSUDSTADDR);
	NAME(NWIOSUDSADDR);
	NAME(NWIOGUDSADDR);
	NAME(NWIOGUDSPADDR);
	NAME(NWIOSUDSTYPE);
	NAME(NWIOSUDSBLOG);
	NAME(NWIOSUDSCONN);
	NAME(NWIOSUDSSHUT);
	NAME(NWIOSUDSPAIR);
	NAME(NWIOSUDSACCEPT);
	NAME(NWIOSUDSCTRL);
	NAME(NWIOGUDSCTRL);
	NAME(NWIOGUDSSOTYPE);
	NAME(NWIOGUDSPEERCRED);
	NAME(NWIOGUDSSNDBUF);
	NAME(NWIOSUDSSNDBUF);
	NAME(NWIOGUDSRCVBUF);
	NAME(NWIOSUDSRCVBUF);
	}

	return NULL;
}

static const struct flags ipopt_flags[] = {
	FLAG_ZERO(NWIO_NOFLAGS),
	FLAG_MASK(NWIO_ACC_MASK, NWIO_EXCL),
	FLAG_MASK(NWIO_ACC_MASK, NWIO_SHARED),
	FLAG_MASK(NWIO_ACC_MASK, NWIO_COPY),
	FLAG(NWIO_EN_LOC),
	FLAG(NWIO_DI_LOC),
	FLAG(NWIO_EN_BROAD),
	FLAG(NWIO_DI_BROAD),
	FLAG(NWIO_REMSPEC),
	FLAG(NWIO_REMANY),
	FLAG(NWIO_PROTOSPEC),
	FLAG(NWIO_PROTOANY),
	FLAG(NWIO_HDR_O_SPEC),
	FLAG(NWIO_HDR_O_ANY),
	FLAG(NWIO_RWDATONLY),
	FLAG(NWIO_RWDATALL),
};

static void
put_ipaddr(struct trace_proc * proc, const char * name, ipaddr_t ipaddr)
{
	struct in_addr in;

	if (!valuesonly) {
		in.s_addr = ipaddr;

		/* Is this an acceptable encapsulation? */
		put_value(proc, name, "[%s]", inet_ntoa(in));
	} else
		put_value(proc, name, "0x%08x", ntohl(ipaddr));
}

static void
put_ipproto(struct trace_proc * proc, const char * name, ipproto_t proto)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (proto) {
		TEXT(IPPROTO_ICMP);
		TEXT(IPPROTO_TCP);
		TEXT(IPPROTO_UDP);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%u", proto);
}

static const struct flags tcpconf_flags[] = {
	FLAG_ZERO(NWTC_NOFLAGS),
	FLAG_MASK(NWTC_ACC_MASK, NWTC_EXCL),
	FLAG_MASK(NWTC_ACC_MASK, NWTC_SHARED),
	FLAG_MASK(NWTC_ACC_MASK, NWTC_COPY),
	FLAG_MASK(NWTC_LOCPORT_MASK, NWTC_LP_UNSET),
	FLAG_MASK(NWTC_LOCPORT_MASK, NWTC_LP_SET),
	FLAG_MASK(NWTC_LOCPORT_MASK, NWTC_LP_SEL),
	FLAG(NWTC_SET_RA),
	FLAG(NWTC_UNSET_RA),
	FLAG(NWTC_SET_RP),
	FLAG(NWTC_UNSET_RP),
};

#define put_port(proc, name, port) \
	put_value(proc, name, "%u", ntohs(port))

static const struct flags tcpcl_flags[] = {
	FLAG_ZERO(TCF_DEFAULT),
	FLAG(TCF_ASYNCH),
};

static const struct flags tcpopt_flags[] = {
	FLAG_ZERO(NWTO_NOFLAG),
	FLAG(NWTO_SND_URG),
	FLAG(NWTO_SND_NOTURG),
	FLAG(NWTO_RCV_URG),
	FLAG(NWTO_RCV_NOTURG),
	FLAG(NWTO_BSD_URG),
	FLAG(NWTO_NOTBSD_URG),
	FLAG(NWTO_DEL_RST),
	FLAG(NWTO_BULK),
	FLAG(NWTO_NOBULK),
};

static const struct flags udpopt_flags[] = {
	FLAG_ZERO(NWUO_NOFLAGS),
	FLAG_MASK(NWUO_ACC_MASK, NWUO_EXCL),
	FLAG_MASK(NWUO_ACC_MASK, NWUO_SHARED),
	FLAG_MASK(NWUO_ACC_MASK, NWUO_COPY),
	FLAG_MASK(NWUO_LOCPORT_MASK, NWUO_LP_SET),
	FLAG_MASK(NWUO_LOCPORT_MASK, NWUO_LP_SEL),
	FLAG_MASK(NWUO_LOCPORT_MASK, NWUO_LP_ANY),
	FLAG(NWUO_EN_LOC),
	FLAG(NWUO_DI_LOC),
	FLAG(NWUO_EN_BROAD),
	FLAG(NWUO_DI_BROAD),
	FLAG(NWUO_RP_SET),
	FLAG(NWUO_RP_ANY),
	FLAG(NWUO_RA_SET),
	FLAG(NWUO_RA_ANY),
	FLAG(NWUO_RWDATONLY),
	FLAG(NWUO_RWDATALL),
	FLAG(NWUO_EN_IPOPT),
	FLAG(NWUO_DI_IPOPT),
};

static void
put_family(struct trace_proc * proc, const char * name, int family)
{
	const char *text = NULL;

	if (!valuesonly) {
		/* TODO: add all the other protocols */
		switch (family) {
		TEXT(AF_UNSPEC);
		TEXT(AF_LOCAL);
		TEXT(AF_INET);
		TEXT(AF_INET6);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", family);
}

static const struct flags sock_type[] = {
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_STREAM),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_DGRAM),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_RAW),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_RDM),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_SEQPACKET),
	FLAG(SOCK_CLOEXEC),
	FLAG(SOCK_NONBLOCK),
	FLAG(SOCK_NOSIGPIPE),
};

static void
put_shutdown_how(struct trace_proc * proc, const char * name, int how)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (how) {
		TEXT(SHUT_RD);
		TEXT(SHUT_WR);
		TEXT(SHUT_RDWR);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", how);
}

static void
put_struct_uucred(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct uucred cred;

	if (!put_open_struct(proc, name, flags, addr, &cred, sizeof(cred)))
		return;

	put_value(proc, "cr_uid", "%u", cred.cr_uid);
	if (verbose > 0) {
		put_value(proc, "cr_gid", "%u", cred.cr_gid);
		if (verbose > 1)
			put_value(proc, "cr_ngroups", "%d", cred.cr_ngroups);
		put_groups(proc, "cr_groups", PF_LOCADDR,
		    (vir_bytes)&cred.cr_groups, cred.cr_ngroups);
	}

	put_close_struct(proc, verbose > 0);
}

static void
put_cmsg_type(struct trace_proc * proc, const char * name, int type)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (type) {
		TEXT(SCM_RIGHTS);
		TEXT(SCM_CREDS);
		TEXT(SCM_TIMESTAMP);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", type);
}

static void
put_msg_control(struct trace_proc * proc, struct msg_control * ptr)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	size_t len;
	int i;

	if (ptr->msg_controllen > sizeof(ptr->msg_control)) {
		put_field(proc, NULL, "..");

		return;
	}

	put_open(proc, NULL, PF_NONAME, "[", ", ");

	memset(&msg, 0, sizeof(msg));
	msg.msg_control = ptr->msg_control;
	msg.msg_controllen = ptr->msg_controllen;

	/*
	 * TODO: decide if we need a verbosity-based limit here.  The argument
	 * in favor of printing everything is that upon receipt, SCM_RIGHTS
	 * actually creates new file descriptors, which is pretty essential in
	 * terms of figuring out what is happening in a process.  In addition,
	 * these calls should be sufficiently rare that the lengthy output is
	 * not really disruptive for the general output flow.
	 */
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		put_open(proc, NULL, 0, "{", ", ");

		if (verbose > 0)
			put_value(proc, "cmsg_len", "%u", cmsg->cmsg_len);
		if (!valuesonly && cmsg->cmsg_level == SOL_SOCKET)
			put_field(proc, "cmsg_level", "SOL_SOCKET");
		else
			put_value(proc, "cmsg_level", "%d", cmsg->cmsg_level);
		if (cmsg->cmsg_level == SOL_SOCKET)
			put_cmsg_type(proc, "cmsg_type", cmsg->cmsg_type);

		len = cmsg->cmsg_len - CMSG_LEN(0);

		/* Print the contents of the messages that we know. */
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			put_open(proc, NULL, PF_NONAME, "[", ", ");
			for (i = 0; i < len / sizeof(int); i++)
				put_fd(proc, NULL,
				    ((int *)CMSG_DATA(cmsg))[i]);
			put_close(proc, "]");
		} else if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_CREDS) {
			put_struct_uucred(proc, NULL, PF_LOCADDR,
			    (vir_bytes)CMSG_DATA(cmsg));
		} else if (len > 0)
			put_field(proc, NULL, "..");

		put_close(proc, "}");
	}

	put_close(proc, "]");
}

int
net_ioctl_arg(struct trace_proc * proc, unsigned long req, void * ptr, int dir)
{
	const char *text;
	nwio_ipopt_t *ipopt;
	nwio_tcpconf_t *nwtc;
	nwio_tcpcl_t *nwtcl;
	nwio_tcpopt_t *nwto;
	tcp_cookie_t *cookie;
	nwio_udpopt_t *nwuo;
	struct sockaddr_un *sun;
	int i;

	switch (req) {
	case FIONREAD:
		/*
		 * Arguably this does not belong here, but as of writing, the
		 * network services are the only ones actually implementing
		 * support for this IOCTL, and we don't have a more suitable
		 * place to put it either.
		 */
		if (ptr == NULL)
			return IF_IN;

		put_value(proc, NULL, "%d", *(int *)ptr);
		return IF_ALL;

	case NWIOSIPOPT:
	case NWIOGIPOPT:
		if ((ipopt = (nwio_ipopt_t *)ptr) == NULL)
			return dir;

		put_flags(proc, "nwio_flags", ipopt_flags, COUNT(ipopt_flags),
		    "0x%x", ipopt->nwio_flags);

		if (ipopt->nwio_flags & NWIO_REMSPEC)
			put_ipaddr(proc, "nwio_rem", ipopt->nwio_rem);
		if (ipopt->nwio_flags & NWIO_PROTOSPEC)
			put_ipproto(proc, "nwio_proto", ipopt->nwio_proto);

		return 0; /* TODO: the remaining fields */

	case NWIOSTCPCONF:
	case NWIOGTCPCONF:
		if ((nwtc = (nwio_tcpconf_t *)ptr) == NULL)
			return dir;

		put_flags(proc, "nwtc_flags", tcpconf_flags,
		    COUNT(tcpconf_flags), "0x%x", nwtc->nwtc_flags);

		/* The local address cannot be set, just retrieved. */
		if (req == NWIOGTCPCONF)
			put_ipaddr(proc, "nwtc_locaddr", nwtc->nwtc_locaddr);

		if ((nwtc->nwtc_flags & NWTC_LOCPORT_MASK) == NWTC_LP_SET)
			put_port(proc, "nwtc_locport", nwtc->nwtc_locport);

		if (nwtc->nwtc_flags & NWTC_SET_RA)
			put_ipaddr(proc, "nwtc_remaddr", nwtc->nwtc_remaddr);

		if (nwtc->nwtc_flags & NWTC_SET_RP)
			put_port(proc, "nwtc_remport", nwtc->nwtc_remport);

		return IF_ALL;

	case NWIOTCPCONN:
	case NWIOTCPLISTEN:
		if ((nwtcl = (nwio_tcpcl_t *)ptr) == NULL)
			return dir;

		put_flags(proc, "nwtcl_flags", tcpcl_flags,
		    COUNT(tcpcl_flags), "0x%x", nwtcl->nwtcl_flags);

		/* We pretend the unused nwtcl_ttl field does not exist. */
		return IF_ALL;

	case NWIOSTCPOPT:
	case NWIOGTCPOPT:
		if ((nwto = (nwio_tcpopt_t *)ptr) == NULL)
			return dir;

		put_flags(proc, "nwto_flags", tcpopt_flags,
		    COUNT(tcpopt_flags), "0x%x", nwto->nwto_flags);
		return IF_ALL;

	case NWIOTCPLISTENQ:
	case NWIOSUDSBLOG:
		if (ptr == NULL)
			return IF_OUT;

		put_value(proc, NULL, "%d", *(int *)ptr);
		return IF_ALL;

	case NWIOGTCPCOOKIE:
	case NWIOTCPACCEPTTO:
		if ((cookie = (tcp_cookie_t *)ptr) == NULL)
			return dir;

		put_value(proc, "tc_ref", "%"PRIu32, cookie->tc_ref);
		if (verbose > 0)
			put_buf(proc, "tc_secret", PF_LOCADDR,
			    (vir_bytes)&cookie->tc_secret,
			    sizeof(cookie->tc_secret));
		return (verbose > 0) ? IF_ALL : 0;

	case NWIOTCPGERROR:
		if (ptr == NULL)
			return IF_IN;

		i = *(int *)ptr;
		if (!valuesonly && (text = get_error_name(i)) != NULL)
			put_field(proc, NULL, text);
		else
			put_value(proc, NULL, "%d", i);
		return IF_ALL;

	case NWIOSUDPOPT:
	case NWIOGUDPOPT:
		if ((nwuo = (nwio_udpopt_t *)ptr) == NULL)
			return dir;

		put_flags(proc, "nwuo_flags", udpopt_flags,
		    COUNT(udpopt_flags), "0x%x", nwuo->nwuo_flags);

		/* The local address cannot be set, just retrieved. */
		if (req == NWIOGUDPOPT)
			put_ipaddr(proc, "nwuo_locaddr", nwuo->nwuo_locaddr);

		if ((nwuo->nwuo_flags & NWUO_LOCPORT_MASK) == NWUO_LP_SET)
			put_port(proc, "nwuo_locport", nwuo->nwuo_locport);

		if (nwuo->nwuo_flags & NWUO_RA_SET)
			put_ipaddr(proc, "nwuo_remaddr", nwuo->nwuo_remaddr);

		if (nwuo->nwuo_flags & NWUO_RP_SET)
			put_port(proc, "nwuo_remport", nwuo->nwuo_remport);

		return IF_ALL;

	case NWIOGUDSFADDR:
	case NWIOSUDSTADDR:
	case NWIOSUDSADDR:
	case NWIOGUDSADDR:
	case NWIOGUDSPADDR:
	case NWIOSUDSCONN:
	case NWIOSUDSACCEPT:
		if ((sun = (struct sockaddr_un *)ptr) == NULL)
			return dir;

		put_family(proc, "sun_family", sun->sun_family);

		/* This could be extended to a generic sockaddr printer.. */
		if (sun->sun_family == AF_LOCAL) {
			put_buf(proc, "sun_path", PF_LOCADDR | PF_PATH,
			    (vir_bytes)&sun->sun_path, sizeof(sun->sun_path));
			return IF_ALL; /* skipping sun_len, it's unused */
		} else
			return 0;

	case NWIOSUDSTYPE:
	case NWIOGUDSSOTYPE:
		if (ptr == NULL)
			return dir;

		put_flags(proc, NULL, sock_type, COUNT(sock_type), "0x%x",
		    *(int *)ptr);
		return IF_ALL;

	case NWIOSUDSSHUT:
		if (ptr == NULL)
			return IF_OUT;

		put_shutdown_how(proc, NULL, *(int *)ptr);
		return IF_ALL;

	case NWIOSUDSPAIR:
		if (ptr == NULL)
			return IF_OUT;

		put_dev(proc, NULL, *(dev_t *)ptr);
		return IF_ALL;

	case NWIOSUDSCTRL:
		if (ptr == NULL)
			return IF_OUT;

		/* FALLTHROUGH */
	case NWIOGUDSCTRL:
		if (ptr == NULL)
			return IF_IN;

		put_msg_control(proc, (struct msg_control *)ptr);
		return IF_ALL;

	case NWIOGUDSPEERCRED:
		if (ptr == NULL)
			return IF_IN;

		put_struct_uucred(proc, NULL, PF_LOCADDR, (vir_bytes)ptr);
		return IF_ALL;

	case NWIOGUDSSNDBUF:
	case NWIOSUDSSNDBUF:
	case NWIOGUDSRCVBUF:
	case NWIOSUDSRCVBUF:
		if (ptr == NULL)
			return dir;

		put_value(proc, NULL, "%zu", *(size_t *)ptr);
		return IF_ALL;

	default:
		return 0;
	}
}
