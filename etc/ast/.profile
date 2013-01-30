# Login shell profile.

# Activate emacs keybindings and command line history support
set -o emacs

# Check terminal type.
case $TERM in
dialup|unknown|network)
	echo -n "Terminal type? ($TERM) "; read term
	TERM="${term:-$TERM}"
	unset term
esac

# Shell configuration.
unset EDITOR; . $HOME/.ashrc
