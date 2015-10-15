.PATH.S: ${.PARSEDIR}
CPPFLAGS+=	-DBN_LLONG="long long"
.include "../../bn.inc"
