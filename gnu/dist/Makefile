# LSC FIXME: Hackish way to trigger the fetch phase from here
# 	     in the same way as from tools.

.include <bsd.own.mk>

MODULE=	texinfo

.include "${.CURDIR}/../../tools/Makefile.gnuhost"

# Force the mapping to standard targets even when not building tools
# We considere work to be done as soon as the fetch step is done,
# as everything else has to be triggered from /tools or ../usr.bin

.if ${USETOOLS} != "yes"
realall realinstall: ${GNUHOSTDIST:H}/.gitignore

.endif

