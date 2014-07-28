#!/bin/sh

set -e

# grep message type constants and make them into a .h file
(
	cat ../include/minix/callnr.h | \
	tr -s ' \t' ' ' | \
	sed 's/^# /#/' | \
	egrep '^#define [A-Z_][A-Z0-9_]* \((PM|VFS)_BASE \+ *[0-9]+\)'

	cat ../include/minix/com.h | \
	tr -s ' \t' ' ' | \
	sed 's/^# /#/' | \
	egrep '^#define [A-Z_][A-Z0-9_]* \( ?([A-Z0-9_]+_BASE|KERNEL_CALL) ?\+[A-Za-z0-9_ +]+\)'	
	
) | cut -d' ' -f2 | sed 's/\(.*\)/IDENT(\1)/' | sort
