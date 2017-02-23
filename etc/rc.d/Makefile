# $NetBSD: Makefile,v 1.93 2015/03/30 10:58:37 chopps Exp $

.include <bsd.own.mk>

#
# NOTE:
#
# If you're adding new scripts, don't forget to update following:
#	src/distrib/sets/lists/etc/mi
#	src/etc/mtree/special
#	src/usr.sbin/postinstall/postinstall
#
# You can find a script to visualize the dependency graph in
# src/sbin/rcorder.

# MINIX: the idea for importing rc.d scripts is basically:
# - in principle, import only the scripts for functionality that we support;
# - do not change dependencies ("REQUIRE", "BEFORE") in such scripts; rather,
#   satisfy the dependencies, recursively, with the following rules;
# - for each such dependency, if it can be disabled (= it has a "rcvar"), then
#   import its script as is, even if we do not support its functionality;
# - if such an unsupported dependency is enabled by default as per
#   etc/defaults/rc.conf, then disable it by overriding it explicitly in
#   etc/defaults/minix.rc.conf;
# - for unsupported dependencies that can not be disabled, create a stub for it
#   (examples: fsck, root, mountcritlocal) but whenever relevant do try to keep
#   its dependency listing intact so as not to disturb the regular ordering.
CONFIGFILES=\
		DAEMON DISKS LOGIN NETWORKING SERVERS \
		\
		bootconf.sh \
		\
		dhclient dhcpcd dhcpd dhcrelay downinterfaces \
		fsck ftpd \
		\
		\
		inetd ipfilter ipsec \
		\
		\
		local \
		\
		mountcritlocal mountcritremote \
		named network npf \
		\
		pwcheck \
		\
		\
		root rtadvd \
		\
		\
		staticroute sysctl sysdb syslogd \
		ttys \
		wscons \
		

.if defined(__MINIX)
# MINIX: we will gradually switch over to the NetBSD rc system.  For conflict
# avoidance, all MINIX-specific scripts should have "minix" in their name.
CONFIGFILES+=	minixrc
.endif

FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

.if !defined(__MINIX) # XXX needs testing first
.if ${MKX11} != "no"
CONFIGFILES+=	xdm xfs fccache
FILESBUILD_xdm=	yes
FILESBUILD_xfs=	yes
FILESBUILD_fccache= yes
.endif # !defined(__MINIX)

.SUFFIXES: .in
.in:
	${_MKTARGET_CREATE}
	${TOOL_SED} -e 's,@X11ROOTDIR@,${X11ROOTDIR},g' < ${.IMPSRC} > ${.TARGET}
.endif

.include <bsd.prog.mk>
