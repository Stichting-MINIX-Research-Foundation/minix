#

.if !defined(_MINIX_PKGSRC_HOOKS_)
_MINIX_PKGSRC_HOOKS_=1

# Get PKGS_REQUIRING_MINIX_LIBC
.include "/usr/pkgsrc/minix/pkgs_requiring_minix_libc.mk"

# Get MINIX_LIBC_* and NETBSD_LIBC_*
.include "/usr/share/mk/minix.libc.mk"

# Build everything not in PKGS_REQUIRING_MINIX_LIBC with NetBSD libc
PKG_REQUIRES_MINIX_LIBC=no

# Search PKGS_REQUIRING_MINIX_LIBC to see if it needs Minix libc
.for PKG_REQUIRING_MINIX_LIBC in ${PKGS_REQUIRING_MINIX_LIBC}
.if "${PKG_REQUIRING_MINIX_LIBC}" == "${PKGPATH}"
PKG_REQUIRES_MINIX_LIBC=yes
.endif
.endfor

# Set the proper compilation flags
.if ${PKG_REQUIRES_MINIX_LIBC} == yes
CPPFLAGS+=	${MINIX_LIBC_CPPFLAGS}
LDFLAGS+=	${MINIX_LIBC_LDFLAGS}
.else
SED=/bin/sed
.include "/usr/pkgsrc/mk/nbsd_libc.buildlink3.mk"
PREFER_NATIVE+=minlib
PREFER_NATIVE+=compat_minix
PREFER_NATIVE+=nbsd_libc
CPPFLAGS+=	${NETBSD_LIBC_CPPFLAGS}
LDFLAGS+=	${NETBSD_LIBC_LDFLAGS}
.endif

.endif  # !defined(_MINIX_PKGSRC_HOOKS_)
