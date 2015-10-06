#!/bin/sh
#
# Perform a checkout / update the MINIX PXELINUX git repo if needed
#

: ${REPO_URL=https://github.com/boricj/syslinux.git}

. releasetools/fetch.functions
