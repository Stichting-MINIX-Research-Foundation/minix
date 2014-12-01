.include <bsd.own.mk>

.if ${MKIMAGEONLY} == "no"
SUBDIR+=	inet

.  if ${MKLWIP} == "yes"
SUBDIR+=	lwip
.  endif # ${MKLWIP} == "yes"

SUBDIR+=	uds
.endif # ${MKIMAGEONLY} == "no"

.include <bsd.subdir.mk>
