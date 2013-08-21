#	$NetBSD: Makefile,v 1.3 2013/05/30 23:42:19 mrg Exp $

.include <bsd.own.mk>

SUBDIR=	\
	C \
	am_ET.UTF-8 armscii-8 \
	el_GR.UTF-8 \
	en_US.UTF-8 \
	georgian-academy georgian-ps \
	ibm-cp1133 iscii-dev isiri-3342 \
	iso8859-1 iso8859-2 iso8859-3 iso8859-4 iso8859-5 iso8859-6 \
	iso8859-7 iso8859-8 iso8859-9 iso8859-9e iso8859-10 iso8859-11 \
	iso8859-13 iso8859-14 iso8859-15 \
	ja ja.JIS ja.SJIS ja_JP.UTF-8 \
	ko ko_KR.UTF-8 koi8-c koi8-r koi8-u \
	microsoft-cp1251 microsoft-cp1255 microsoft-cp1256 mulelao-1 \
	nokhchi-1 \
	pt_BR.UTF-8 \
	tatar-cyr th_TH th_TH.UTF-8 tscii-0 \
	vi_VN.tcvn vi_VN.viscii \
	zh_CN zh_CN.UTF-8 zh_CN.gb18030 zh_CN.gbk zh_HK.UTF-8 \
	zh_HK.big5 zh_HK.big5hkscs zh_TW zh_TW.UTF-8 zh_TW.big5


LTFILES=compose.dir locale.alias locale.dir

FILESDIR=	${X11LIBDIR}/locale

.for _F in ${LTFILES}
${_F}.lt: ${_F}.pre
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	( \
	${CPP} -undef -traditional \
	    < ${.ALLSRC} | ${X11TOOL_UNXCOMM} \
	    | ${TOOL_SED} -e \
		'/^[^#][^ 	]*:/s/://' -e '/^[^#].*[ 	].*:/d'; \
	${CPP} -undef -traditional \
	    < ${.ALLSRC} | ${X11TOOL_UNXCOMM} ; \
	) > ${.TARGET}

FILES+=			${_F}.lt
FILESNAME_${_F}.lt=	${_F}
CLEANFILES+=		${_F}.lt
.endfor

realall: ${FILES}

.PATH: ${X11SRCDIR.X11}/nls

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.subdir.mk>

#
# build all the silly subdirs
#
build_subdirs:
.for _dir in ${SUBDIR}
	mkdir ${.CURDIR}/${_dir}
	printf '#	$$NetBSD: Makefile,v 1.3 2013/05/30 23:42:19 mrg Exp $$\n\nNLS_SUBDIR=	${_dir}\n\n.include "../Makefile.nls"\n' > ${.CURDIR}/${_dir}/Makefile

.endfor
