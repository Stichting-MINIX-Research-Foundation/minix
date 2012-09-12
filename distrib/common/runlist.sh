#	$NetBSD: runlist.sh,v 1.1 2009/09/18 09:24:59 abs Exp $

if [ "X$1" = "X-d" ]; then
	SHELLCMD=cat
	shift
else
	SHELLCMD="sh -e"
fi

( while [ "X$1" != "X" ]; do
	cat $1
	shift
done ) | awk -f ${NETBSDSRCDIR}/distrib/common/list2sh.awk | ${SHELLCMD}
