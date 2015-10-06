#!/bin/sh
#
# Perform a checkout / update the MINIX u-boot git repo if needed
#

: ${REPO_URL=git://git.minix3.org/u-boot}

. releasetools/fetch.functions
