# $Id: unexport-env.mk,v 1.1 2014/08/21 13:44:52 apb Exp $

# pick up a bunch of exported vars
.include "export.mk"

# an example of setting up a minimal environment.
PATH = /bin:/usr/bin:/sbin:/usr/sbin

# now clobber the environment to just PATH and UT_TEST
UT_TEST = unexport-env

# this removes everything
.unexport-env
.export PATH UT_TEST
