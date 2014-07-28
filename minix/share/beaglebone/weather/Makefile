# BeagleBone Weather Cape Demo
.include <bsd.own.mk>

NOOBJ=	# defined

FILES=	LICENSE README.txt jquery.js style.css weatherstation.js \
	index.html processing.js spin.js weatherstation.lua

# weatherstation.lua is an executable script run under tcpd and needs BINMODE
FILESMODE_weatherstation.lua=${BINMODE}

FILESDIR=/usr/share/beaglebone/weather

.include <bsd.prog.mk>
