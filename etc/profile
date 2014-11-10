#	$NetBSD: profile,v 1.1 1997/06/21 06:07:39 mikel Exp $
#
# System-wide .profile file for sh(1).

# MINIX specifics
# Set library path
export LD_LIBRARY_PATH="/lib:/usr/lib:/usr/X11R7/lib:/usr/pkg/lib:/usr/local/lib"

# Set the timezone
export TZ=GMT0
RC_TZ=/etc/rc.timezone

if [ -f ${RC_TZ} ]; then
	. ${RC_TZ}
fi

export TZ
