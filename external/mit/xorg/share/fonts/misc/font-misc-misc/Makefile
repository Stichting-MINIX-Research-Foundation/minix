#	$NetBSD: Makefile,v 1.2 2012/08/20 15:38:58 tron Exp $

.include <bsd.own.mk>

FONTSUBDIR=	misc
FONTSRCPKG=	font-misc-misc

.PATH:  	${X11SRCDIR.${FONTSRCPKG}}

BDFFILES+=	12x13ja.bdf 18x18ja.bdf
BDFFILES+=	18x18ko.bdf

BDFFILES+=	k14.bdf nil2.bdf

BDFFILES+=	4x6.bdf 5x7.bdf 5x8.bdf 6x10.bdf 6x12.bdf 6x13.bdf \
		6x13B.bdf 6x13O.bdf 6x9.bdf 7x13.bdf 7x13B.bdf 7x13O.bdf \
		7x14.bdf 7x14B.bdf 8x13.bdf 8x13B.bdf 8x13O.bdf 9x15.bdf \
		9x15B.bdf 9x18.bdf 9x18B.bdf 10x20.bdf

# Most fonts have 22 output types, but ISO8859-8 and ISO8859-11 are
# missing a few.
NO_ISO8859-8=	6x13O 7x13O 8x13O
.for _s in ${NO_ISO8859-8}
NO_UCSFILES+=	${_s}-ISO8859-8
.endfor

NO_ISO8859-11=	4x6 5x7 5x8 6x9 6x10 6x12 6x13B 6x13O 8x13 8x13B 8x13O 9x18B
.for _s in ${NO_ISO8859-11}
NO_UCSFILES+=	${_s}-ISO8859-11
.endfor

NO_KOI8-R=	6x13B 6x13O 7x13B 7x13O 7x14B 8x13B 8x13O 9x15B 9x18B
.for _s in ${NO_KOI8-R}
NO_UCSFILES+=	${_s}-KOI8-R
.endfor

ISOLOCSTD=	ISO8859-1 ISO8859-2 ISO8859-3 ISO8859-4 ISO8859-5 ISO8859-7 \
		ISO8859-8 ISO8859-9 ISO8859-10 ISO8859-11 ISO8859-13 \
		ISO8859-14 ISO8859-15 ISO8859-16 KOI8-R
# .. and those 22 outputs
ISOSIZESTD=	4x6 \
		5x7 5x8 \
		6x9 6x10 6x12 6x13 6x13B 6x13O \
		7x13 7x13B 7x13O 7x14 7x14B \
		8x13 8x13B 8x13O \
		9x15 9x15B 9x18 9x18B \
		10x20

.for _l in ${ISOLOCSTD}
.for _s in ${ISOSIZESTD}
.if empty(NO_UCSFILES:M${_s}-${_l})
UCSFILES+=	${_s} ${_l}
.endif
.endfor
.endfor

UCSFILES+=	7x14 JISX0201.1976-0

.include "../../Makefile.bdf"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
