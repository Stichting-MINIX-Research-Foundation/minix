#	$NetBSD: dot.login,v 1.3 2003/04/24 01:02:26 perry Exp $
#
# This is the default .login file.
# Users are expected to edit it to meet their own needs.
#
# The commands in this file are executed when a csh user first
# logs in.  This file is processed after .cshrc.
#
# See csh(1) for details.
#

if ( ! $?SHELL ) then
  setenv SHELL /bin/csh
endif

set noglob
eval `tset -s -m 'network:?xterm'`
unset noglob
stty status '^T' crt -tostop

if ( -x /usr/games/fortune ) /usr/games/fortune
