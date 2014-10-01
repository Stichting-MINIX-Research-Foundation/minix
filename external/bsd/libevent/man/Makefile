# $NetBSD: Makefile,v 1.2 2013/04/11 17:49:12 christos Exp $

# Try to undo the doxygen lossage in the man pages.

HSRCS1= \
buffer.h.3 \
buffer_compat.h.3 \
dns.h.3 \
dns_compat.h.3 \
http.h.3 \
http_compat.h.3 \
rpc.h.3 \
rpc_compat.h.3 \
tag.h.3 \
tag_compat.h.3 \
thread.h.3 \
util.h.3

HMAN1=${HSRCS1:S/^/ev/g:S/.h.3/.3/g}

.for i in ${HSRCS1}
ev${i:S/.h.3/.3/g}: $i
	${.CURDIR}/fixman ${TOOL_SED} ${.ALLSRC} ${.TARGET}
.endfor

HSRCS2= \
bufferevent_ssl.h.3 \
event.h.3 \
event_compat.h.3

.for i in ${HSRCS2}
${i:S/.h.3/.3/g}: $i
	${.CURDIR}/fixman ${TOOL_SED} ${.ALLSRC} ${.TARGET}
.endfor

HMAN2=${HSRCS2:S/.h.3/.3/g}

SRCS1=deprecated.3
MAN1=${SRCS1:S/^/ev/g}

.for i in ${SRCS1}
ev$i: $i
	${TOOL_SED}  -e 's/"deprecated"/"evdeprecated"/g' \
		-e 's/^deprecated/^evdeprecated/g' < ${.ALLSRC} > ${.TARGET}
.endfor

MAN+= \
bufferevent.3 \
evbuffer_cb_info.3 \
evbuffer_iovec.3 \
evbuffer_ptr.3 \
event_base.3 \
event_config.3 \
evthread_condition_callbacks.3 \
evthread_lock_callbacks.3 \
evutil_addrinfo.3

MAN+= ${HMAN1} ${HMAN2} ${MAN1}

CLEANFILES+=${HMAN1} ${HMAN2} ${MAN1}

.include <bsd.man.mk>
