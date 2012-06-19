.include <bsd.own.mk>
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

pkgsrc-create: git
	@echo "If problems occur you may have to rm -rf pkgsrc and try again."
	@echo ""
	${INSTALL_DIR} ${.CURDIR}/pkgsrc
	cd ${.CURDIR}/pkgsrc && git init
	cd ${.CURDIR}/pkgsrc && \
		git remote add origin git://git.minix3.org/pkgsrc.git
	cd ${.CURDIR}/pkgsrc && git fetch origin
	cd ${.CURDIR}/pkgsrc && git branch minix-master origin/minix-master
	cd ${.CURDIR}/pkgsrc && git checkout minix-master
	cd ${.CURDIR}/pkgsrc && git pull
	cd ${.CURDIR}/pkgsrc/minix && sh minibootstrap.sh

pkgsrc-checkout: git
	cd ${.CURDIR}/pkgsrc && git checkout minix-master
	cd ${.CURDIR}/pkgsrc/minix && sh minibootstrap.sh

pkgsrc-update: git
	cd ${.CURDIR}/pkgsrc && git pull
	cd ${.CURDIR}/pkgsrc/minix && sh minibootstrap.sh

git:
	pkgin update
	pkgin install scmgit-base
