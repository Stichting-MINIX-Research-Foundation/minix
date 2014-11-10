#	$NetBSD: dot.profile,v 1.26 2012/03/11 17:28:47 he Exp $

export PATH=/sbin:/usr/sbin:/bin:/usr/bin:/usr/pkg/sbin:/usr/pkg/bin
export PATH=${PATH}:/usr/X11R7/bin:/usr/X11R6/bin:/usr/local/sbin:/usr/local/bin

# Uncomment the following line(s) to install binary packages
# from ftp.NetBSD.org via pkg_add.  (See also pkg_install.conf)
#export PKG_PATH=ftp://ftp.NetBSD.org/pub/pkgsrc/packages/NetBSD/$(uname -m)/6.0/All
#export PKG_PATH="${PKG_PATH};ftp://ftp.NetBSD.org/pub/pkgsrc/packages/NetBSD/$(uname -m)/5.1/All"
#export PKG_PATH="${PKG_PATH};ftp://ftp.NetBSD.org/pub/pkgsrc/packages/NetBSD/$(uname -m)/5.0/All"

export BLOCKSIZE=1k

export HOST="$(hostname)"

if [ -x /usr/bin/tset ]; then
	eval $(tset -sQrm 'unknown:?unknown')
fi

umask 022
#ulimit -c 0

export ENV=/root/.shrc

# Do not display in 'su -' case
# Would be nice, but still not tested enough on MINIX
#if [ -z "$SU_FROM" ]; then
#        echo "We recommend that you create a non-root account and use su(1) for root access."
#fi
