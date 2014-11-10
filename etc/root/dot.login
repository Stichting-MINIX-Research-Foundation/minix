#	$NetBSD: dot.login,v 1.9 2009/05/15 23:57:50 ad Exp $

eval `tset -sQrm 'unknown:?unknown'`

# Do not display in 'su -' case
if ( ! $?SU_FROM ) then
	echo "We recommend that you create a non-root account and use su(1) for root access."
endif
