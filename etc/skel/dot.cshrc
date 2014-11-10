#	$NetBSD: dot.cshrc,v 1.7 2011/10/19 14:42:37 christos Exp $
#
# This is the default .cshrc file.
# Users are expected to edit it to meet their own needs.
#
# The commands in this file are executed each time a new csh shell
# is started.
#
# See csh(1) for details.
#

# Set your editor. Default to explicitly setting vi, as otherwise some
# software will run ed and other software will fail. Can be set to
# emacs or nano or whatever other editor you may prefer, but of course
# those editors must be installed before you can use them.
setenv	EDITOR	vi

# vi settings: set show-match auto-indent always-redraw shift-width=4
#setenv	EXINIT	"se sm ai redraw sw=4"

# VISUAL sets the "visual" editor, i.e., vi rather than ed, which if
# set will be run by preference to $EDITOR by some software. It is
# mostly historical and usually does not need to be set.
#setenv	VISUAL	${EDITOR}

# Set the pager. This is used by, among other things, man(1) for
# showing man pages. The default is "more". Another reasonable choice
# (included with the system by default) is "less".
#setenv	PAGER	more

# Set your default printer, if desired.
#setenv	PRINTER	change-this-to-a-printer

# Set the search path for programs.
set path = (~/bin /bin /sbin /usr/{bin,sbin,X11R7/bin,X11R6/bin,pkg/{,s}bin,games} \
	    /usr/local/{,s}bin)

if ($?prompt) then
	# An interactive shell -- set some stuff up

	# Filename completion.
	set filec

	# Size of the history buffer.
	set history = 1000

	# Do not exit on EOF condition (e.g. ^D typed)
	# (disabled by default, not default behavior)
	#set ignoreeof

	# Set the location of your incoming email for mail notification.
	set mail = (/var/mail/$USER)

	# Set the prompt to include the hostname.
	set mch = `hostname -s`
	set prompt = "${mch:q}: {\!} "
endif
