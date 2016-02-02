# $Id: unexport.mk,v 1.1 2014/08/21 13:44:52 apb Exp $

# pick up a bunch of exported vars
.include "export.mk"

.unexport UT_ZOO UT_FOO

UT_TEST = unexport
