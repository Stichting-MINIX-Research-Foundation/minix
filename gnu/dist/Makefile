# LSC FIXME: Hackish way to trigger the fetch phase from here
# 	     in the same way as from tools.

.include <bsd.own.mk>

MODULE=	texinfo

.include "${.CURDIR}/../../tools/Makefile.gnuhost"

# Force the mapping to standard targets even when not building tools
.if ${USETOOLS} != "yes"
realall: .build_done
realinstall: .install_done
.endif
