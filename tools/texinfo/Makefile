#	$NetBSD: Makefile,v 1.11 2013/06/14 16:10:02 tsutsui Exp $

MODULE=		texinfo

CONFIGURE_ARGS=	--program-prefix=${_TOOL_PREFIX}

.include "${.CURDIR}/../Makefile.gnuhost"

# Apply host compatibility patches before configuring the fetched
# Texinfo 4.8 source. This is required for Linux-hosted cross-builds too.
TEXINFO_GLIBC_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-glibc-strcasecmp
TEXINFO_GETENV_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-glibc-getenv
TEXINFO_SYSTEM_STDLIB_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-system-header-stdlib
TEXINFO_XEXIT_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-xexit-stdlib
TEXINFO_XMALLOC_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-xmalloc-stdlib
TEXINFO_MKSTEMP_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-mkstemp-prototype
TEXINFO_CMDS_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-makeinfo-cmds-stdlib
TEXINFO_DEFUN_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-makeinfo-defun-stdlib
TEXINFO_FILES_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-makeinfo-files-stdlib
TEXINFO_FLOAT_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-makeinfo-float-stdlib
TEXINFO_FOOTNOTE_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-makeinfo-footnote-stdlib
TEXINFO_HTML_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-makeinfo-html-stdlib
TEXINFO_C23_PATCH=${NETBSDSRCDIR}/gnu/usr.bin/texinfo/patches/patch-zz-texinfo-c23-command-functions
TEXINFO_CMDS_H=${GNUHOSTDIST}/makeinfo/cmds.h
TEXINFO_SYSTEM_H=${GNUHOSTDIST}/lib/system.h
TEXINFO_XEXIT_C=${GNUHOSTDIST}/lib/xexit.c
TEXINFO_XMALLOC_C=${GNUHOSTDIST}/lib/xmalloc.c
TEXINFO_MKSTEMP_C=${GNUHOSTDIST}/lib/mkstemp.c
TEXINFO_CMDS_C=${GNUHOSTDIST}/makeinfo/cmds.c
TEXINFO_DEFUN_C=${GNUHOSTDIST}/makeinfo/defun.c
TEXINFO_FILES_C=${GNUHOSTDIST}/makeinfo/files.c
TEXINFO_FLOAT_C=${GNUHOSTDIST}/makeinfo/float.c
TEXINFO_FOOTNOTE_C=${GNUHOSTDIST}/makeinfo/footnote.c
TEXINFO_HTML_C=${GNUHOSTDIST}/makeinfo/html.c

.if exists(${TEXINFO_SYSTEM_H}) && exists(${TEXINFO_GLIBC_PATCH}) && \
    exists(${TEXINFO_GETENV_PATCH}) && exists(${TEXINFO_SYSTEM_STDLIB_PATCH}) && \
    exists(${TEXINFO_XEXIT_C}) && \
    exists(${TEXINFO_XEXIT_PATCH}) && exists(${TEXINFO_XMALLOC_C}) && \
    exists(${TEXINFO_XMALLOC_PATCH}) && exists(${TEXINFO_MKSTEMP_C}) && \
    exists(${TEXINFO_MKSTEMP_PATCH}) && exists(${TEXINFO_CMDS_C}) && exists(${TEXINFO_CMDS_H}) && \
    exists(${TEXINFO_CMDS_PATCH}) && exists(${TEXINFO_DEFUN_C}) && \
    exists(${TEXINFO_DEFUN_PATCH}) && exists(${TEXINFO_FILES_C}) && \
    exists(${TEXINFO_FILES_PATCH}) && exists(${TEXINFO_FLOAT_C}) && \
    exists(${TEXINFO_FLOAT_PATCH}) && exists(${TEXINFO_FOOTNOTE_C}) && \
    exists(${TEXINFO_FOOTNOTE_PATCH}) && exists(${TEXINFO_HTML_C}) && \
    exists(${TEXINFO_HTML_PATCH}) && exists(${TEXINFO_C23_PATCH})
.texinfo_host_compat_done: ${TEXINFO_SYSTEM_H} ${TEXINFO_SYSTEM_STDLIB_PATCH} ${TEXINFO_XEXIT_C} \
    ${TEXINFO_XMALLOC_C} ${TEXINFO_MKSTEMP_C} ${TEXINFO_CMDS_C} \
    ${TEXINFO_CMDS_H} ${TEXINFO_DEFUN_C} ${TEXINFO_FILES_C} ${TEXINFO_FLOAT_C} ${TEXINFO_FOOTNOTE_C} ${TEXINFO_HTML_C} ${TEXINFO_GLIBC_PATCH} ${TEXINFO_GETENV_PATCH} ${TEXINFO_XEXIT_PATCH} \
    ${TEXINFO_XMALLOC_PATCH} ${TEXINFO_MKSTEMP_PATCH} ${TEXINFO_CMDS_PATCH} \
    ${TEXINFO_DEFUN_PATCH} ${TEXINFO_FILES_PATCH} ${TEXINFO_FLOAT_PATCH} ${TEXINFO_FOOTNOTE_PATCH} ${TEXINFO_HTML_PATCH} ${TEXINFO_C23_PATCH}
	@if grep -Fq 'extern int strcasecmp ();' ${TEXINFO_SYSTEM_H}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_GLIBC_PATCH}); fi
	@if grep -Fq 'extern char *getenv ();' ${TEXINFO_SYSTEM_H}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_GETENV_PATCH}); fi
	@if ! grep -Fq 'Ensure standard allocation declarations are available without STDC_HEADERS.' ${TEXINFO_SYSTEM_H}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_SYSTEM_STDLIB_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_XEXIT_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_XEXIT_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_XMALLOC_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_XMALLOC_PATCH}); fi
	@if grep -Fq 'int __gen_tempname ();' ${TEXINFO_MKSTEMP_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_MKSTEMP_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_CMDS_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_CMDS_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_DEFUN_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_DEFUN_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_FILES_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_FILES_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_FLOAT_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_FLOAT_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_FOOTNOTE_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_FOOTNOTE_PATCH}); fi
	@if ! grep -Fq '#include <stdlib.h>' ${TEXINFO_HTML_C}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_HTML_PATCH}); fi
	@if ! grep -Fq 'typedef void COMMAND_FUNCTION (int, int, int);' ${TEXINFO_CMDS_H}; then (cd ${GNUHOSTDIST} && patch -p1 < ${TEXINFO_C23_PATCH}); fi
	@touch ${.TARGET}

.configure_done: .texinfo_host_compat_done
.endif

.if !empty(.MAKE.OS:M*CYGWIN*) && target(install)
.END: install-texinfo
install-texinfo: 
	${HOST_SH} ${NETBSDSRCDIR}/tools/binstall/mkmanifest ${TOOLDIR}/bin/${_TOOL_PREFIX}install-info.exe
.endif
