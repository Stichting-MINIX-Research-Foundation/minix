#!/bin/sh
#
svc=`basename $1 ,S`,S
if test \( ! -r $svc \) -a -d "SVC" ; then svc=SVC/$svc ; fi
grep '^#\*\*\*SVC' $svc

