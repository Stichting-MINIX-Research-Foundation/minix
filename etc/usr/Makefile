# Makefile - installed as /usr/Makefile
#
# Provides simple targets to download and maintain /usr/pkgsrc and /usr/src.

########################################################################
# Entry point
########################################################################
all help: usage

MINIX_RELEASE!=	uname -r

########################################################################
# PKGSRC
########################################################################
PKGSRC_REPO_DIR:= ${.CURDIR}/pkgsrc
PKGSRC_REPO_URL:= git://git.minix3.org/pkgsrc-ng.git
PKGSRC_BRANCH:=	${MINIX_RELEASE}
MSG_RENAMED_BRANCH:= "\
\n======================================================================== \
\nWarning: \
\n  Renaming $$$${current_name} to $$$${new_name}. \
\n======================================================================== \
"
MSG_RESET_PKGSRC:= "\
\n======================================================================== \
\nNote: \
\n  If problems occur you may have to rm -rf ${PKGSRC_REPO_DIR} \
\n  and try again. \
\n======================================================================== \
"

.if exists(${PKGSRC_REPO_DIR}/.git)
# We can only update the repository if it *does* exist
pkgsrc-update: git
	# If we can fast-forward, just do it, otherwise rename current branch
	# and checkout the official branch (also warn the user).
	@cd ${PKGSRC_REPO_DIR} && git pull --ff-only || \
	( current_name=$$(git rev-parse --abbrev-ref HEAD); \
	  new_name=$${current_name}-$$(git rev-parse --short HEAD); \
	  echo -e ${MSG_RENAMED_BRANCH}; \
	  echo -n "Do you want to continue [Y/n]? "; \
	  read ok; \
	  if [ "$${ok}" = "" -o "$${ok}" = "y" -o "$${ok}" = "Y" ]; then \
	    git branch -m $${new_name}; \
	    git checkout -t origin/${PKGSRC_BRANCH}; \
	  else \
	    exit 1; \
	  fi; \
	)
	@cd ${PKGSRC_REPO_DIR}/minix && sh minibootstrap.sh

.if !exists(${PKGSRC_REPO_DIR}/Makefile)
# If the .git directory is available, but not yet checked out, then
pkgsrc-checkout: git
	@cd ${PKGSRC_REPO_DIR} && git checkout ${PKGSRC_BRANCH}
	@cd ${PKGSRC_REPO_DIR}/minix && sh minibootstrap.sh

.endif # !exists(${PKGSRC_REPO_DIR}/Makefile)
.else
# We can only create the repository if it does *not* exist
pkgsrc: git
	@echo -e ${MSG_RESET_PKGSRC}
	@git clone -b ${PKGSRC_BRANCH} ${PKGSRC_REPO_URL} ${PKGSRC_REPO_DIR}
	@cd ${PKGSRC_REPO_DIR}/minix && sh minibootstrap.sh

.endif # exists(${PKGSRC_REPO_DIR}/.git)

########################################################################
# MINIX SRC
########################################################################
SRC_REPO_DIR:= ${.CURDIR}/src
SRC_REPO_URL:= git://git.minix3.org/minix.git
SRC_BRANCH:= master
MSG_WRONG_BRANCH_SRC:= "\
\n======================================================================== \
\nERROR: \
\n  An unknown branch is presently checked out, please checkout \
\n  first the following branch: ${SRC_BRANCH} \
\n======================================================================== \
"
MSG_RESET_SRC:= "\
\n======================================================================== \
\nNote: \
\n  If problems occur you may have to rm -rf ${SRC_REPO_DIR} \
\n  and try again. \
\n======================================================================== \
"
.if exists(${SRC_REPO_DIR}/.git)
# We can only update the repository if it *does* exist
src-update: git
	@cd ${SRC_REPO_DIR} && \
		[ "$$(git rev-parse --abbrev-ref HEAD)" = "${SRC_BRANCH}" ] || \
		( echo -e ${MSG_WRONG_BRANCH_SRC}; exit 1 )
	@cd ${SRC_REPO_DIR} && git pull

.if !exists(${SRC_REPO_DIR}/Makefile)
src-checkout: git
	@cd ${SRC_REPO_DIR} && git checkout ${SRC_BRANCH}
.endif # !exists(${SRC_REPO_DIR}/Makefile)
.else
# We can only create the repository if it does not exist
src: git
	@echo -e ${MSG_RESET_SRC}
	@git clone ${SRC_REPO_URL} ${SRC_REPO_DIR}

.endif # exists(${SRC_REPO_DIR}/.git)

########################################################################
# Support rules
########################################################################
git:
	@pkgin -y update 2>&1 > /dev/null
	@pkgin -y install git-base 2>&1 > /dev/null

usage:
	@echo "Usage: make [target]"
	@echo ""
	@echo " Where target is one of:"
	@echo "    help, all, usage	 - print this help"
	@echo ""

.if exists(${SRC_REPO_DIR}/.git)
	@echo "    make src-update	 - update the src repo from the net"
.if !exists(${SRC_REPO_DIR}/Makefile)
	@echo "    make src-checkout	 - initial checkout of your pre-packaged"
	@echo "				   pkgsrc repo."
.endif # !exists(${SRC_REPO_DIR}/Makefile)
.else
	@echo "    make src		 - fetch initial src repo from the net"
.endif # exists(${SRC_REPO_DIR}/.git)
	@echo ""

.if exists(${PKGSRC_REPO_DIR}/.git)
.if !exists(${PKGSRC_REPO_DIR}/Makefile)
	@echo "    make pkgsrc-checkout - initial checkout of your pre-packaged"
	@echo "				  pkgsrc repo."
.endif # !exists(${PKGSRC_REPO_DIR}/Makefile)
	@echo "    make pkgsrc-update	 - update your pkgsrc repo from the net"
.else
	@echo "    make pkgsrc		 - fetch initial pkgsrc repo from the net"
.endif # exists(${PKGSRC_REPO_DIR}/.git)
	@echo ""

_debug:
	@echo "Settings:"
.for var in MINIX_RELEASE \
	SRC_REPO_DIR SRC_REPO_URL SRC_BRANCH \
	PKGSRC_REPO_DIR PKGSRC_REPO_URL PKGSRC_BRANCH
	@echo "  ${var} = ${${var}}"

.endfor

