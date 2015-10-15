#	$NetBSD: Makefile,v 1.20 2015/07/20 04:10:40 mrg Exp $

.include <bsd.own.mk>

SUBDIR=	config

.PATH:	${X11SRCDIR.freetype}/include

INCSDIR=${X11INCDIR}/freetype2

INCS=	freetype.h ftadvanc.h ftbbox.h ftbdf.h ftbitmap.h ftbzip2.h ftcache.h \
	ftchapters.h ftcid.h fterrdef.h fterrors.h ftfntfmt.h ftgasp.h \
	ftglyph.h ftgxval.h ftgzip.h ftimage.h ftincrem.h ftlcdfil.h ftlist.h \
	ftlzw.h ftmac.h ftmm.h ftmodapi.h ftmoderr.h ftotval.h ftoutln.h \
	ftpfr.h ftrender.h ftsizes.h ftsnames.h ftstroke.h ftsynth.h \
	ftsystem.h fttrigon.h fttypes.h ftwinfnt.h \
	t1tables.h ttnameid.h tttables.h tttags.h ttunpat.h \
	ftautoh.h ftcffdrv.h ft2build.h

SCRIPTS=	freetype-config

CLEANFILES+=	${SCRIPTS}

FT_VERSION!=	sed -n '/^version_info=/{s/version_info=.\([0-9]*\):\([0-9]*\):\([0-9]*\)./\1.\2.\3/;p;q;}' < ${X11SRCDIR.freetype}/builds/unix/configure.ac

# XXX hijack config-subst from Xft
CONFIG_SUBST=	${TOOL_SED} \
		-e 's,%prefix%,${X11ROOTDIR},' \
		-e 's,%exec_prefix%,${X11BINDIR},' \
		-e 's,%libdir%,${X11USRLIBDIR},g' \
		-e 's,%includedir%,${X11INCDIR},' \
		-e 's,%ft_version%,${FT_VERSION},' \
		-e 's,%LIBS_CONFIG%,-lfreetype -lz -lbz2,' \
		-e 's,%LIBSSTATIC_CONFIG%,-lfreetype -lz -lbz2,' \
		-e 's,%build_libtool_libs%,yes,' \
		-e 's@%hardcode_libdir_flag_spec%@-Wl,-rpath,${X11USRLIBDIR}@' \
		-e 's,%FT2_EXTRA_LIBS%,,' \
		-e 's,%LIBZ%,-lz,' \
		-e 's,%LIBBZ2%,-lbz2,' \
		-e 's,@FT2_EXTRA_LIBS@,,' \
		-e 's,@LIBZ@,-lz,' \
		-e 's,@LIBBZ2@,-lbz2,'

.PATH: ${X11SRCDIR.freetype}/builds/unix
freetype-config: freetype-config.in
	${_MKTARGET_CREATE}
	${CONFIG_SUBST} < ${.ALLSRC} > ${.TARGET}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>
