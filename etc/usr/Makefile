# Makefile - installed as /usr/Makefile
#
# Provides simple targets to download and maintain /usr/pkgsrc etc.

help all:
	@echo "HELP:"
	@echo ""
.if exists(${.CURDIR}/pkgsrc/.git)
.if exists(${.CURDIR}/pkgsrc/Makefile)
	@echo "    make pkgsrc-update       - update your pkgsrc repo from the net"
.else
	@echo "    make pkgsrc-checkout     - initial checkout of your pre-packaged"
	@echo "                               pkgsrc repo."
	@echo "    make pkgsrc-update       - update your pkgsrc repo from the net"
	@echo "                               after the initial checkout."
.endif
.else
	@echo "    make pkgsrc-create       - fetch initial pkgsrc repo from the net"
	@echo "    make pkgsrc-update       - update your pkgsrc repo from the net"
.endif
	@echo ""

pkgsrc-create:
	@echo "If problems occur you may have to rm -rf pkgsrc and try again."
	@echo ""
	mkdir -p ${.CURDIR}/pkgsrc
	cd ${.CURDIR}/pkgsrc && git init
	cd ${.CURDIR}/pkgsrc && \
		git remote add origin http://github.com/gautambt/Pkgsrc-Minix.git
	cd ${.CURDIR}/pkgsrc && git fetch origin
	cd ${.CURDIR}/pkgsrc && git branch minix origin/minix
	cd ${.CURDIR}/pkgsrc && git checkout minix
	cd ${.CURDIR}/pkgsrc && git pull

pkgsrc-checkout:
	cd ${.CURDIR}/pkgsrc && git checkout minix

pkgsrc-update:
	cd ${.CURDIR}/pkgsrc && git pull
