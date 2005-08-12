# Login shell profile.

# Erase character and erase line interrupt keys
stty sane erase '^H' kill '^U'

# Check terminal type.
case $TERM in
dialup|unknown|network)
	echo -n "Terminal type? ($TERM) "; read term
	TERM="${term:-$TERM}"
	unset term
esac

# Shell configuration.
unset EDITOR; . $HOME/.ashrc
