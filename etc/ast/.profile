# Login shell profile.

# Erase character, erase line, and interrupt keys.
stty sane erase '^H' kill '^U' intr '^?'

# Check terminal type.
case $TERM in
dialup|unknown|network)
	echo -n "Terminal type? ($TERM) "; read term
	TERM="${term:-$TERM}"
	unset term
esac

# Shell configuration.
unset EDITOR; . $HOME/.ashrc
