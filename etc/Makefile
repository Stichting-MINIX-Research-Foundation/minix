#	$NetBSD: Makefile,v 1.398 2012/08/17 16:59:26 riz Exp $
#	from: @(#)Makefile	8.7 (Berkeley) 5/25/95

# Environment variables without default values:
#   DESTDIR must be set before anything in this file will work.
#   RELEASEDIR is where the tarred up stuff for a snapshot or
#	release will be placed.
#
# Environment variables with default values:
#   LOCALTIME will set the default local time for the system you
#	build; it determines what /etc/localtime is symlink'd to.
#   KERNSRCDIR points to kernel source; it is set by default to ../sys,
#	but can be overridden.
#   KERNOBJDIR is the kernel build directory, it defaults to
#	${KERNSRCDIR}/arch/${MACHINE}/compile, but can be overridden.
#   KERNCONFDIR is where the configuration files for kernels are found;
#	default is ${KERNSRCDIR}/arch/${MACHINE}/conf but can be overridden.
#   MKCRYPTO; if not `no', install crypto-related configuration
#   MKPOSTFIX; if not `no', install postfix configuration
#   MKUNPRIVED; if not `no', allow non-root installs.
#   MKUPDATE; if not `no', don't do a 'make clean' before kernel compile
#
# Targets:
#    distribution: makes a full NetBSD distribution in DESTDIR. If
#	INSTALL_DONE is set, it will not do a `make install.'
#	if DISTRIBUTION_DONE is set, it will not do anything.
#    distrib-dirs: creates an empty NetBSD directory tree in DESTDIR.
#	Called by distribution.
#    snapshot: calls distribution, above, and then tars up the files
#	into a release(7) format in RELEASEDIR/${RELEASEMACHINEDIR}.
#	Any port-dependent stuff for this target is found in
#	etc.${MACHINE}/Makefile.inc.
#    release: a synonym for `snapshot'
#

# For MK* vars
.include <bsd.own.mk>

.include <bsd.sys.mk>		# for HOST_SH, TOOL_AWK, ...
.include <bsd.kernobj.mk>	# For KERNSRCDIR, KERNOBJDIR, ...
.include <bsd.endian.mk>	# For TARGET_ENDIANNESS

.MAKEOVERRIDES+=	USETOOLS

TZDIR=		/usr/share/zoneinfo
LOCALTIME?=	UTC
CKSUM?=		${TOOL_CKSUM}
MAKESUMS=	MAKE=${MAKE:Q} CKSUM=${CKSUM:Q} ${HOST_SH} ${NETBSDSRCDIR}/distrib/sets/makesums
DISTRIBVER!=	${HOST_SH} ${NETBSDSRCDIR}/sys/conf/osrelease.sh

# Flags for creating ISO CDROM image
# mkisofs is expected to be in $PATH, install via pkgsrc/sysutils/cdrtools
# Note: At least mkisofs 2.0 should be used.
#
.if !defined(__MINIX)
# LSC: We do not yet support iso creation anyway, then do not try to find it.
.if !defined(MKISOFS)
MKISOFS!=       (which mkisofs || echo true)
.endif
DISTRIBREV!=	${HOST_SH} ${KERNSRCDIR}/conf/osrelease.sh -s
# ISO 9660 volume ID.  Note that this can only contain [A-Z0-9_].
ISO_VOLID!=	echo NETBSD_${DISTRIBREV} | tr a-z A-Z
MKISOFS_FLAGS+= -J -l -hide-joliet-trans-tbl -r -T \
		-V ${ISO_VOLID} \
		-publisher "The NetBSD Project" \
		-m "${RELEASEDIR}/${RELEASEMACHINEDIR}/installation/cdrom"
.if ${MKISOFS_FLAGS:N-v}
MKISOFS_FLAGS+=	-quiet
.endif
.endif # !defined(__MINIX)
 

# MD Makefile.inc may append MD targets to BIN[123].  Make sure all
# are empty, to preserve the old semantics of setting them below with "+=".
#
BIN1=
BIN2=
BIN3=

# Directories to build in ${RELEASEDIR}/${RELEASEMACHINEDIR}.
# MD Makefile.inc files can add to this.
# NOTE: Parent directories must be listed before subdirectories.
#
INSTALLATION_DIRS=	binary binary/sets binary/kernel installation

.if exists(etc.${RELEASEMACHINE}/Makefile.inc)
.include "etc.${RELEASEMACHINE}/Makefile.inc"
.endif

# -rw-r--r--
BINOWN= root
BINGRP= operator
UTMPGRP= utmp
.if defined(__MINIX)
BIN1+=	boot.cfg.default \
	man.conf \
	protocols rc rc.cd rc.subr \
	rc.daemons.dist rs.inet rs.single \
	services shells syslog.conf \
	termcap utmp
.else
BIN1+=	bootptab changelist csh.cshrc csh.login \
	csh.logout daily daily.conf dm.conf envsys.conf floppytab ftpchroot \
	ftpusers gettytab gpio.conf group hosts hosts.lpd inetd.conf \
	locate.conf login.conf mailer.conf man.conf monthly monthly.conf \
	mrouted.conf named.conf netconfig networks newsyslog.conf \
	nsswitch.conf ntp.conf passwd.conf phones printcap profile protocols \
	rbootd.conf rc rc.conf rc.local rc.subr rc.shutdown remote rpc \
	security security.conf services shells shrc sysctl.conf syslog.conf \
	weekly weekly.conf wscons.conf

# Use machine-specific disktab if it exists, or the MI one otherwise
.if exists(etc.${MACHINE}/disktab)
BIN1+=	etc.${MACHINE}/disktab
.else
BIN1+=	disktab
.endif

.if exists(etc.${MACHINE}/ld.so.conf)
BIN1+=	etc.${MACHINE}/ld.so.conf
.endif

.if exists(etc.${MACHINE}/ttyaction)
BIN1+=	etc.${MACHINE}/ttyaction
.endif

# -rw-rw-r--
.if !empty(DISTRIBVER:M*.99.*)
BIN2+=	motd.current
FILESNAME_motd.current=	motd
.elif !empty(DISTRIBVER:M*BETA*)
BIN2+=	motd.beta
FILESNAME_motd.beta=	motd
.elif !empty(DISTRIBVER:M*RC*)
BIN2+=	motd.rc
FILESNAME_motd.rc=	motd
.elif !empty(DISTRIBVER:M*STABLE*)
BIN2+=	motd.stable
FILESNAME_motd.stable=	motd
.else
BIN2+=	motd
.endif

# -rw-------
BIN3+=	hosts.equiv
.endif # defined(__MINIX)

SYSPKG=	etc
ETC_PKG=-T etc_pkg
BASE_PKG=-T base_pkg
ETC_INSTALL_FILE=cd ${.CURDIR} && ${INSTALL_FILE} ${ETC_PKG}
ETC_INSTALL_OBJ_FILE=cd ${.OBJDIR} && ${INSTALL_FILE} ${ETC_PKG}

.if ${TARGET_ENDIANNESS} == "1234"
PWD_MKDB_ENDIAN=	-L
.elif ${TARGET_ENDIANNESS} == "4321"
PWD_MKDB_ENDIAN=	-B
.else
PWD_MKDB_ENDIAN=
.endif


# distribution --
#	Build a distribution
#
distribution: .PHONY .MAKE check_DESTDIR distrib-dirs
.if !defined(DISTRIBUTION_DONE)
.if !defined(INSTALL_DONE)
	${MAKEDIRTARGET} ${NETBSDSRCDIR} include _DISTRIB=
	${MAKEDIRTARGET} ${NETBSDSRCDIR} install _DISTRIB=
.endif	# !INSTALL_DONE
	${MAKEDIRTARGET} . install-etc-files
. if ${MKX11} != "no"
.  if ${X11FLAVOUR} == "Xorg"
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/external/mit/xorg distribution
.  else
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/x11 distribution
.  endif
. endif
. if ${MKEXTSRC} != "no"
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/extsrc distribution
. endif
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/distrib/sets makesetfiles
.endif	# !DISTRIBUTION_DONE


CLEANFILES+=	MAKEDEV
.if !defined(__MINIX)
MAKEDEV: ${.CURDIR}/MAKEDEV.awk ${.CURDIR}/MAKEDEV.tmpl \
    ${.CURDIR}/etc.${MACHINE}/MAKEDEV.conf
	${_MKTARGET_CREATE}
	MACHINE=${MACHINE:Q} MACHINE_ARCH=${MACHINE_ARCH:Q} \
	    NETBSDSRCDIR=${NETBSDSRCDIR:Q} \
	    ${TOOL_AWK} -f ${.CURDIR}/MAKEDEV.awk ${.CURDIR}/MAKEDEV.tmpl \
	    > ${.TARGET}
.else
MAKEDEV:
	if [ ${MKUNPRIVED} != "yes" ]; then \
		cd ${DESTDIR}/dev && sh ${NETBSDSRCDIR}/commands/MAKEDEV/MAKEDEV.sh null ;\
		cd ${DESTDIR}/dev && sh ${NETBSDSRCDIR}/commands/MAKEDEV/MAKEDEV.sh std  2>/dev/null ;\
	fi
.endif # !defined(__MINIX)

RELEASEVARS=	BSDOBJDIR BSDSRCDIR BUILDID \
		DESTDIR EXTERNAL_TOOLCHAIN HAVE_GCC HAVE_GDB \
		INSTALLWORLDDIR \
		KERNARCHDIR KERNCONFDIR KERNOBJDIR KERNSRCDIR \
		MACHINE MACHINE_ARCH MAKE MAKECONF MAKEFLAGS \
		MAKEOBJDIR MAKEOBJDIRPREFIX MAKEVERBOSE \
		MKBFD MKBINUTILS MKCATPAGES \
		MKCRYPTO MKCRYPTO_RC5 MKCVS \
		MKDEBUG MKDEBUGLIB MKDOC MKDTRACE MKDYNAMICROOT \
		MKGCC MKGCCCMDS MKGDB \
		MKHESIOD MKHTML MKIEEEFP MKINET6 MKINFO MKIPFILTER \
		MKKERBEROS MKLDAP MKLINKLIB MKLINT \
		MKMAN MKMANZ MKMDNS MKNLS MKNPF MKOBJ MKOBJDIRS \
		MKPAM MKPF MKPIC MKPICINSTALL MKPICLIB MKPOSTFIX MKPROFILE \
		MKSHARE MKSKEY MKSOFTFLOAT MKSTATICLIB \
		MKUNPRIVED MKUPDATE MKX11 MKYP \
		NBUILDJOBS NETBSDSRCDIR \
		NOCLEANDIR NODISTRIBDIRS NOINCLUDES \
		OBJMACHINE \
		RELEASEDIR RELEASEMACHINEDIR TOOLCHAIN_MISSING TOOLDIR \
		USE_HESIOD USE_INET6 USE_JEMALLOC USE_KERBEROS USE_LDAP \
		USE_PAM USE_SKEY USE_YP \
		USETOOLS USR_OBJMACHINE \
		X11SRCDIR X11FLAVOUR

params: .PHONY
.for var in ${RELEASEVARS}
.if defined(${var})
	@printf "%20s = '%-s'\n" ${var} ${${var}:Q}
.else
	@printf "%20s = (undefined)\n" ${var}
.endif
.endfor

CLEANFILES+=	etc-release
etc-release: .EXEC .MAKE
	${_MKTARGET_CREATE}
	@(	echo "NetBSD ${DISTRIBVER}/${MACHINE}"; \
		echo ; \
		cat ${NETBSDSRCDIR}/sys/conf/copyright; \
		echo ; \
		echo "Build settings:"; \
		printf "%20s   %s\n"  "Build date" "$$(date -u)"; \
		printf "%20s   %s\n"  "Built by" "$${USER-root}@$$(hostname)"; \
		echo ; \
		(cd ${.CURDIR}; ${MAKE} ${MFLAGS} -j1 params); \
	) >${.OBJDIR}/${.TARGET}

install-etc-release: .PHONY etc-release
	${_MKMSG_INSTALL} etc/release
	${ETC_INSTALL_OBJ_FILE} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    etc-release ${DESTDIR}/etc/release


FILESDIR=		/etc
CONFIGFILES=
CONFIGSYMLINKS=

.for file in ${BIN1}
CONFIGFILES+=		${file}
FILESMODE_${file:T}=	644
.endfor

.for file in ${BIN2}
CONFIGFILES+=		${file}
FILESMODE_${file:T}=	664
.endfor

.for file in ${BIN3}
CONFIGFILES+=		${file}
FILESMODE_${file:T}=	600
.endfor

.if (${MKPOSTFIX} != "no")
CONFIGFILES+=		aliases
FILESDIR_aliases=	/etc/mail
FILESMODE_aliases=	644
.endif

.if !defined(__MINIX)
CONFIGFILES+=		MAKEDEV.local
FILESDIR_MAKEDEV.local=	/dev
FILESMODE_MAKEDEV.local=${BINMODE}

CONFIGFILES+=		crontab
FILESDIR_crontab=	/var/cron/tabs
FILESNAME_crontab=	root
FILESMODE_crontab=	600

CONFIGFILES+=		minfree
FILESDIR_minfree=	/var/crash
FILESMODE_minfree=	600

CONFIGSYMLINKS+=	${TZDIR}/${LOCALTIME}	/etc/localtime \
			/usr/sbin/rmt		/etc/rmt
.else
CONFIGSYMLINKS+=	\
			/usr/lib/cpp		/lib/cpp \
			/usr/log		/var/log \
			/usr/tmp		/var/tmp \
			/usr/pkg/bin/clang	/usr/bin/cc \
			/usr/pkg/bin/strip	/usr/bin/strip \
			/dev/kbdaux		/dev/mouse \
			/proc/mounts		/etc/mtab

.endif # !defined(__MINIX)


# install-etc-files --
#	Install etc (config) files; not performed by "make build"
#
install-etc-files: .PHONY .MAKE check_DESTDIR MAKEDEV
	${_MKMSG_INSTALL} ${DESTDIR}/etc/master.passwd
	${ETC_INSTALL_FILE} -o root -g operator -m 600 \
	    master.passwd ${DESTDIR}/etc
	${TOOL_PWD_MKDB} -p ${PWD_MKDB_ENDIAN} -d ${DESTDIR}/ \
	    ${DESTDIR}/etc/master.passwd
.if ${MKUNPRIVED} != "no"
	( \
		for metaent in passwd pwd.db spwd.db; do \
	    		echo "./etc/$${metaent} type=file mode=0644 uname=root gname=operator tags=etc_pkg"; \
		done; \
	) | ${METALOG.add}
.endif	# MKUNPRIVED != no
.if defined(__MINIX)
# BJG: Unsafe (i.e. user-editable) files for Minix
.for owner group mode sdir tdir files in \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ group \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ hostname.file \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ inet.conf \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ mk.conf \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ resolv.conf \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ motd \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ rc.conf \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ profile 
	${_MKMSG_INSTALL} ${DESTDIR}${tdir}${files:T}
	if [ ! -e ${tdir} ]; then \
		${INSTALL_DIR} ${tdir}; \
	fi; \
	${INSTALL_FILE} -o ${owner} -g ${group} -m ${mode} ${sdir}${files} ${tdir};
.endfor
# LSC: We need a safe install target for etc files, as this is expected from
# our current user base. This safe version only leaves out the master.passwd
# file in order not to loose any user account created.
#
# BJG: For Minix, the -safe target *is* performed by "make build"
#
# LSC: To ensure minimal modifications, the logic is a bit contrived, i.e. the
# NetBSD build system expect install-etc-files to be unsafe, so what was done
# is to separate that step into two steps, with the unsafe version refering 
# (not by dependency, to ensure correct order of actions) the safe step, and 
# use the following mapping in the main Makefile:
#  etcforce -> install-etc-files (default NetBSD target)
#  etcfiles -> install-etc-files-safe (new -safe- target)
	${MAKEDIRTARGET} . install-etc-files-safe

install-etc-files-safe: .PHONY .MAKE check_DESTDIR MAKEDEV
# LSC Minix administrator group is operator, not wheel
.endif # defined(__MINIX)
	${_MKMSG_INSTALL} ${DESTDIR}/etc/ttys
.if !defined(__MINIX)
	${ETC_INSTALL_OBJ_FILE} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/etc.${MACHINE}/ttys ${DESTDIR}/etc
.else
	${ETC_INSTALL_OBJ_FILE} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/ttys ${DESTDIR}/etc
.endif # !defined(__MINIX)
.if exists(etc.${MACHINE}/boot.cfg)
	${_MKMSG_INSTALL} ${DESTDIR}/boot.cfg
	${ETC_INSTALL_OBJ_FILE} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/etc.${MACHINE}/boot.cfg ${DESTDIR}/
.endif
.if !defined(__MINIX)
	${_MKMSG_INSTALL} ${DESTDIR}/dev/MAKEDEV
	${ETC_INSTALL_OBJ_FILE} -o ${BINOWN} -g ${BINGRP} -m 555 \
	    MAKEDEV ${DESTDIR}/dev
.for owner group mode file in \
		${BINOWN} operator	664	/etc/dumpdates  \
		${BINOWN} operator	600	/etc/skeykeys \
		root wheel		600	/var/at/at.deny \
		root wheel		644	/var/db/locate.database \
		${BINOWN} ${BINGRP}	600	/var/log/authlog \
		root wheel		600	/var/log/cron \
		${BINOWN} ${UTMPGRP}	664	/var/log/lastlog \
		${BINOWN} ${UTMPGRP}	664	/var/log/lastlogx \
		${BINOWN} ${BINGRP}	640	/var/log/lpd-errs \
		${BINOWN} ${BINGRP}	600	/var/log/maillog \
		${BINOWN} ${BINGRP}	644	/var/log/messages \
		${BINOWN} ${BINGRP}	600	/var/log/secure \
		${BINOWN} ${UTMPGRP}	664	/var/log/wtmp \
		${BINOWN} ${UTMPGRP}	664	/var/log/wtmpx \
		${BINOWN} ${BINGRP}	600	/var/log/xferlog \
		daemon staff		664	/var/msgs/bounds \
		${BINOWN} ${UTMPGRP}	664	/var/run/utmp \
		${BINOWN} ${UTMPGRP}	664	/var/run/utmpx \
		games games		664	/var/games/atc_score \
		games games		664	/var/games/battlestar.log \
		games games		664	/var/games/cfscores \
		games games		664	/var/games/criblog \
		games games		660	/var/games/hackdir/perm \
		games games		660	/var/games/hackdir/record \
		games games		664	/var/games/larn/llog12.0 \
		games games		664	/var/games/larn/lscore12.0 \
		games games		664	/var/games/larn/playerids \
		games games		664	/var/games/robots_roll \
		games games		664	/var/games/rogue.scores \
		games games		664	/var/games/saillog \
		games games		664	/var/games/snakerawscores \
		games games		664	/var/games/snake.log \
		games games		664	/var/games/tetris.scores
	${_MKMSG_INSTALL} ${DESTDIR}${file}
	if [ ! -e ${DESTDIR}${file} -o -s ${DESTDIR}${file} ]; then \
		${ETC_INSTALL_FILE} -o ${owner} -g ${group} -m ${mode} \
    			/dev/null ${DESTDIR}${file}; \
	else true; fi
.endfor
.for subdir in . defaults bluetooth iscsi mtree namedb pam.d powerd rc.d root skel ssh
	${MAKEDIRTARGET} ${subdir} configinstall
.endfor
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/external/bsd/dhcpcd/sbin/dhcpcd configinstall
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/usr.bin/mail configinstall
.if (${MKPF} != "no")
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/usr.sbin/pf configinstall
.endif
.if (${MKCRYPTO} != "no")
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/crypto/external/bsd/openssh/bin configinstall
.endif
.if (${MKPOSTFIX} != "no")
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/external/ibm-public/postfix configinstall
.endif
.if (${MKATF} != "no")
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/external/bsd/atf/etc/atf configinstall
.endif
.if (${MKKYUA} != "no")
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/external/bsd/kyua-cli/etc/kyua configinstall
.endif
.else # LSC Minix Specific
.for owner group mode sdir tdir files in \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/usr/ ${DESTDIR}/usr/etc/ daily \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/usr/ ${DESTDIR}/usr/etc/ dhcptags.conf \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/usr/ ${DESTDIR}/usr/etc/ rc \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/usr/lib/ crontab \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/ ${DESTDIR}/etc/ system.conf \
		${BINOWN} ${BINGRP}	${NONBINMODE}	${NETBSDSRCDIR}/etc/usr/ ${DESTDIR}/usr/ Makefile \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/devmand/scripts/ ${DESTDIR}/etc/devmand/scripts/ block  \
		${BINOWN} ${BINGRP}	${BINMODE}	${NETBSDSRCDIR}/etc/devmand/scripts/ ${DESTDIR}/etc/devmand/scripts/ singlechar \
		${BINOWN} ${BINGRP}	${NONBINMODE}	${NETBSDSRCDIR}/etc/ast/ ${DESTDIR}/usr/ast/ .[aepv]* \
		${BINOWN} ${BINGRP}	${NONBINMODE}	${NETBSDSRCDIR}/etc/ast/ ${DESTDIR}/root/ .[aepv]* \
		ast	  users		${NONBINMODE}	${NETBSDSRCDIR}/etc/ast/ ${DESTDIR}/home/ast/ .[aepv]* \
		bin	  ${BINGRP}	${NONBINMODE}	${NETBSDSRCDIR}/etc/ast/ ${DESTDIR}/home/bin/ .[aepv]* \

	${_MKMSG_INSTALL} ${DESTDIR}${tdir}${files:T}
	if [ ! -e ${tdir} ]; then \
		${INSTALL_DIR} ${tdir}; \
	fi; \
	${INSTALL_FILE} -o ${owner} -g ${group} -m ${mode} ${sdir}${files} ${tdir};
.endfor
.for subdir in . defaults mtree
	${MAKEDIRTARGET} ${subdir} configinstall
.endfor
	${_MKMSG_INSTALL} ${DESTDIR}/usr/lib/fonts
	${INSTALL_DIR} ${DESTDIR}/usr/lib/fonts
	${INSTALL_FILE} -m ${BINMODE} -o ${BINOWN} -g ${BINGRP} ${NETBSDSRCDIR}/etc/fonts/*.fnt ${DESTDIR}/usr/lib/fonts/
.endif # LSC Minix Specific


# install-obsolete-lists --
#	Install var/db/obsolete set lists; this is performed by "make build"
#
OBSOLETE.dir=		${.OBJDIR}/obsolete.dir
.if !defined(__MINIX)
OBSOLETE.files=		base comp etc games man misc text
.else
OBSOLETE.files=		minix
.endif # defined(__MINIX)
.if ${MKKMOD} != "no"
OBSOLETE.files+=	modules
.endif
.if ${MKATF} != "no"
OBSOLETE.files+=	tests
.endif
.if ${MKX11} != "no"
OBSOLETE.files+=	xbase xcomp xetc xfont xserver
.endif

# XXX make "makeobsolete" set wise; then generate files respectively
install-obsolete-lists: .PHONY .MAKE
	mkdir -p ${OBSOLETE.dir}
.if ${MKX11} != "no"
	(cd ${NETBSDSRCDIR}/distrib/sets && \
	    AWK=${TOOL_AWK:Q} MAKE=${MAKE:Q} ${HOST_SH} ./makeobsolete -b -t ${OBSOLETE.dir})
.else
	(cd ${NETBSDSRCDIR}/distrib/sets && \
	    AWK=${TOOL_AWK:Q} MAKE=${MAKE:Q} ${HOST_SH} ./makeobsolete -t ${OBSOLETE.dir})
.endif
.for file in ${OBSOLETE.files}
	${_MKMSG_INSTALL} ${DESTDIR}/var/db/obsolete/${file}
	if [ ! -e ${DESTDIR}/var/db/obsolete/${file} ] || \
	    ! cmp -s ${OBSOLETE.dir}/${file} ${DESTDIR}/var/db/obsolete/${file}; then \
		${ETC_INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m 644 \
		    ${OBSOLETE.dir}/${file} ${DESTDIR}/var/db/obsolete; \
	else true; fi
.endfor


# distrib-dirs --
#	Populate $DESTDIR with directories needed by NetBSD
#
distrib-dirs: .PHONY check_DESTDIR
	cd ${NETBSDSRCDIR}/etc/mtree && ${MAKE} distrib-dirs


# release, snapshot --
#	Build a full distribution including kernels & install media.
#
release snapshot: .PHONY .MAKE check_DESTDIR check_RELEASEDIR snap_md_post
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/distrib/sets sets
	${MAKESUMS} -A -t ${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/sets \
		${KERNEL_SETS:@.SETS.@kern-${.SETS.}.tgz@}
	${MAKESUMS} -t ${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/kernel '*.gz'


.if !defined(__MINIX)
# iso-image --
#	Standalone target to create a CDROM image after the release
#	was composed.  Should be run after "make release" in src and xsrc.
#	The do-iso-image is to be called from etc.$MACHINE/Makefile.inc
#
#	Note: At least mkisofs 2.0 should be used.
#
CDROM_NAME_ADD?=
CDROM_IMAGE?=${RELEASEDIR}/iso/NetBSD-${DISTRIBVER}-${MACHINE}.iso
CDROM.dir=	${.OBJDIR}/cdrom.dir
CDROM.pathlist=	${.OBJDIR}/cdrom.pathlist

iso-image:

.if ${MKISOFS} != true
do-iso-image: .PHONY check_DESTDIR check_RELEASEDIR iso-image-md-post
	${MAKESUMS} -t ${RELEASEDIR}/iso/ '*.iso'
	@echo "iso-image created as: ${CDROM_IMAGE}"
.else
do-iso-image:
	@echo iso-image: mkisofs not found
.endif

iso-image-setup: .PHONY check_RELEASEDIR
	rm -f ${CDROM.pathlist}
.for extra in README SOURCE_DATE source
.if exists(${RELEASEDIR}/${extra})
	echo "${extra}=${RELEASEDIR}/${extra}" >> ${CDROM.pathlist}
.endif
.endfor
	echo "${MACHINE}/=${RELEASEDIR}/${RELEASEMACHINEDIR}/" >> ${CDROM.pathlist}
	mkdir -p ${CDROM.dir}
	mkdir -p ${RELEASEDIR}/${RELEASEMACHINEDIR}/installation/cdrom

# iso-image-mi --
#	Create the image after the MD operations have completed.
#
iso-image-mi: .PHONY check_DESTDIR check_RELEASEDIR iso-image-md-pre
	@if ! ${MKISOFS} --version; then \
		echo "install pkgsrc/sysutils/cdrtools and run 'make iso-image'." ; \
		false; \
	fi
	${MKISOFS} ${MKISOFS_FLAGS} -graft-points -path-list ${CDROM.pathlist} \
	    -o ${CDROM_IMAGE} ${CDROM.dir}

# iso-image-md-pre --
#	Setup ${CDROM.dir} to produce a bootable CD image.
#	Overridden by etc.$MACHINE/Makefile.inc
#
iso-image-md-pre: .PHONY check_DESTDIR check_RELEASEDIR iso-image-setup
#	(empty -- look in the machine-dependent Makefile.inc)

# iso-image-md-post --
#	Fixup the CD-image to be bootable.
#	Overridden by etc.$MACHINE/Makefile.inc
#
iso-image-md-post: .PHONY check_DESTDIR check_RELEASEDIR iso-image-mi
#	(empty -- look in the machine-dependent Makefile.inc)


# live-image --
#	Standalone target to create live images after the release was composed.
#	Should be run after "make release" in src and xsrc.
#	LIVEIMG_RELEASEDIR specifies where to install live images and
#	it can be set in MD etc.${MACHINE}/Makefile.inc.
#
LIVEIMG_RELEASEDIR?= ${RELEASEDIR}/images

live-image: .PHONY check_DESTDIR check_RELEASEDIR
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/distrib live_image \
	    LIVEIMG_RELEASEDIR=${LIVEIMG_RELEASEDIR}
	${MAKESUMS} -t ${LIVEIMG_RELEASEDIR} '*.img.gz'

# install-image --
#	Standalone target to create installation images
#	after the release was composed.
#	Should be run after "make release" in src and xsrc.
#	INSTIMG_RELEASEDIR specifies where to install live images and
#	it can be set in MD etc.${MACHINE}/Makefile.inc.
#
INSTIMG_RELEASEDIR?= ${RELEASEDIR}/images

install-image: .PHONY check_DESTDIR check_RELEASEDIR
	${MAKEDIRTARGET} ${NETBSDSRCDIR}/distrib install_image \
	    INSTIMG_RELEASEDIR=${INSTIMG_RELEASEDIR}
	${MAKESUMS} -t ${INSTIMG_RELEASEDIR} '*.img.gz'
.endif # !defined(__MINIX)

# snap_pre --
#	Create ${RELEASEDIR} and necessary subdirectories.
#
snap_pre: .PHONY check_DESTDIR check_RELEASEDIR distribution
	${INSTALL} -d -m 755 ${RELEASEDIR}
.if ${MKUPDATE} == "no"
# Could be a mount point, ignore the errors
	-/bin/rm -rf ${RELEASEDIR}/${RELEASEMACHINEDIR}
.endif
	${INSTALL} -d -m 755 ${RELEASEDIR}/${RELEASEMACHINEDIR}
.for dir in ${INSTALLATION_DIRS}
	${INSTALL} -d -m 755 ${RELEASEDIR}/${RELEASEMACHINEDIR}/${dir}
.endfor

# snap_post --
#	Build the install media and notes from distrib
#
snap_post: .PHONY .MAKE build_kernelsets build_releasekernels
.if ${MKUPDATE} == "no"
	cd ${NETBSDSRCDIR}/distrib && ${MAKE} cleandir
.endif
	cd ${NETBSDSRCDIR}/distrib && ${MAKE} depend && ${MAKE} && \
	    ${MAKE} release

# build kernels --
#	This target builds the kernels specified by each port.
#	A port may specify the following kernels:
#
#	KERNEL_SETS		The list of kernels that will be
#				packaged into sets, named
#				kern-${kernel}.tgz.  These kernels
#				are also placed in the binary/kernel
#				area of the release package as
#				netbsd-${kernel}.gz.
#
#	EXTRA_KERNELS		Additional kernels to place in the
#				binary/kernel area of the release
#				package as netbsd-${kernel}.gz, but
#				which are not placed into sets. This
#				allows a port to provide e.g. a netbootable
#				installation kernel containing a ramdisk.
#
#	BUILD_KERNELS		Additional kernels to build which are
#				not placed into sets nor into the
#				binary/kernel area of the release
#				package.  These are typically kernels
#				that are built for inclusion only in
#				installation disk/CD-ROM/tape images.
#
#	A port may also specify KERNEL_SUFFIXES, which is an optional list
#	of filename suffixes for kernels to include in the kernel sets and
#	in the binary/kernel area of the release package (e.g. "netbsd" vs.
#	"netbsd.ecoff" and "netbsd.srec").  It is not an error if kernels
#	with these suffixes do not exist in the kernel build directory.
#
#
# A list of all the kernels to build, which can be overridden from
# external sources (such as make(1)'s environment or command line)
#
ALL_KERNELS?=	${KERNEL_SETS} ${EXTRA_KERNELS} ${BUILD_KERNELS}

GETKERNELAWK=	${TOOL_AWK} '/^config/ {print $$2; found=1} \
		END{ if (found == 0) print "netbsd"; }'

build_kernels: .PHONY
#	Configure & compile kernels listed in ${ALL_KERNELS}
#
# The 'sync' is so that all writes during the build are pushed back
# to the disk.  Not having it causes problems on some host systems
# (e.g. Linux) when building on NFS.
#
.if !defined(KERNELS_DONE)						# {
.for configfile in ${ALL_KERNELS}					# {
build_kernels: kern-${configfile}
kern-${configfile}: .PHONY .MAKE
	cd ${KERNCONFDIR} && ${TOOL_CONFIG} -s ${KERNSRCDIR} \
	    -b ${KERNOBJDIR}/${configfile:C/.*\///} ${configfile}
.if ${MKUPDATE} == "no"
	cd ${KERNOBJDIR}/${configfile:C/.*\///} && ${MAKE} distclean
.endif
	cd ${KERNOBJDIR}/${configfile:C/.*\///} && ${MAKE} depend && ${MAKE}
	sync
.endfor	# ALL_KERNELS							# }
.endif	# KERNELS_DONE							# }

build_kernelsets: .PHONY
#	Create kernel sets from ${KERNEL_SETS} into
#	${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/sets
#
.for configfile in ${KERNEL_SETS}					# {
build_kernelsets: kernset-${configfile}
kernset-${configfile}: .PHONY build_kernels snap_pre
	@ kernlist=$$(${GETKERNELAWK} ${KERNCONFDIR}/${configfile}); \
	kerndir=${KERNOBJDIR}/${configfile:C/.*\///}; \
	kernsuffixes="${KERNEL_SUFFIXES:S/^/./}"; \
	kern_tgz=${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/sets/kern-${configfile}.tgz; \
	pax_cmd="COMPRESS_PROGRAM=${TOOL_GZIP:Q} GZIP=-9 ${TOOL_PAX} -O -zw -M -N ${NETBSDSRCDIR}/etc -f $${kern_tgz}"; \
	cd $${kerndir} && { \
		kernels=; newest=; \
		for kernel in $${kernlist}; do \
			for s in "" $${kernsuffixes}; do \
				ks="$${kernel}$${s}"; \
				[ -f $${ks} ] || continue; \
				kernels="$${kernels} $${ks}"; \
				[ -z "$${newest}" -o $${ks} -nt "$${newest}" ] && \
					newest=$${ks}; \
			done; \
		done; \
		[ $${kern_tgz} -nt "$${newest}" ] || { \
			echo "echo $${kernels} | $${pax_cmd}"; \
			( echo "/set uname=${BINOWN} gname=${BINGRP}"; \
			echo ". type=dir optional"; \
			for kernel in $${kernels}; do \
				echo "./$${kernel} type=file"; \
			done ) | eval $${pax_cmd}; \
		} \
	}
.endfor	# KERNEL_SETS							# }

build_releasekernels: .PHONY
#	Build kernel.gz from ${KERNEL_SETS} ${EXTRA_KERNELS} into
#	${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/kernel
#
.for configfile in ${KERNEL_SETS} ${EXTRA_KERNELS}			# {
build_releasekernels: releasekern-${configfile}
releasekern-${configfile}: .PHONY build_kernels snap_pre
	@ kernlist=$$(${GETKERNELAWK} ${KERNCONFDIR}/${configfile:C/.*\///}); \
	kerndir=${KERNOBJDIR}/${configfile:C/.*\///}; \
	kernsuffixes="${KERNEL_SUFFIXES:S/^/./}"; \
	cd $${kerndir} && {	\
		for kernel in $${kernlist}; do \
			for s in "" $${kernsuffixes}; do \
				ks="$${kernel}$${s}"; \
				[ ! -f $${ks} ] && continue; \
				knl_gz="${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/kernel/$${kernel}-${configfile:C/.*\///}$${s}.gz"; \
				[ $${knl_gz} -nt $${ks} ] && continue; \
				rm -f $${knl_gz}; \
				echo "${TOOL_GZIP} -c -9 < $${kerndir}/$${ks} > $${knl_gz}"; \
				${TOOL_GZIP} -c -9 < $${ks} > $${knl_gz}; \
			done; \
		done; \
	}
.endfor	# KERNEL_SETS EXTRA_KERNELS					# }

# snap_md_post --
#	Machine dependent distribution media operations.
#	Overridden by etc.$MACHINE/Makefile.inc
#
snap_md_post: .PHONY check_DESTDIR check_RELEASEDIR snap_post
#	(empty -- look in the machine-dependent Makefile.inc)


clean:
	-rm -rf ${CDROM.dir} ${CDROM.pathlist} ${OBSOLETE.dir}

.if !defined(__MINIX)
SUBDIR=	defaults rc.d mtree
.else
SUBDIR=	devmand defaults mtree
.endif  # defined(__MINIX)

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

test:
	@echo ${OBSOLETE.files}
