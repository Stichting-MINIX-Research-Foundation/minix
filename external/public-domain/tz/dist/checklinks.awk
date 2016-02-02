# Check links in tz tables.

# Contributed by Paul Eggert.

/^Link/ { used[$2] = 1 }
/^Zone/ { defined[$2] = 1 }

END {
    status = 0

    for (tz in used) {
	if (!defined[tz]) {
	    printf "%s: Link to non-zone\n", tz
	    status = 1
	}
    }

    exit status
}
