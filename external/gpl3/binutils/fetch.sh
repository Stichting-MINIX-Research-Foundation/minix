#!/bin/sh

set -e

# Make sure we're in our directory (i.e., where this shell script is).
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$SCRIPT_DIR"

# Configure fetch method.
URL="http://ftp.gnu.org/gnu/binutils/binutils-2.34.tar.bz2"
BACKUP_URL="https://sourceware.org/pub/binutils/releases/binutils-2.34.tar.bz2"
FETCH=ftp
if command -v curl >/dev/null 2>&1; then
	FETCH="curl -O -f"
fi

fetch_archive()
{
	if [ -f binutils-2.34.tar.bz2 ]; then
		return 0
	fi

	if ! $FETCH "$URL"; then
		$FETCH "$BACKUP_URL"
	fi
}

# Fetch and prepare the sources if they are not available.
if [ ! -d dist ]; then
	fetch_archive
	tar -oxjf binutils-2.34.tar.bz2
	mv binutils-2.34 dist

	(
		cd dist
		# Keep the testsuite source directories required by enabled build
		# components: libiberty/configure needs Makefile.in, binutils builds
		# testsuite/gentestdlls, and Gold configures and builds its testsuite.
		find . -type d -name testsuite \
		    ! -path './libiberty/testsuite' \
		    ! -path './binutils/testsuite' \
		    ! -path './gold/testsuite' -prune -exec rm -fr {} +
		find . -name .gitignore -delete
		# Apply optional downstream patches when the directory is present.
		if [ -d ../patches ]; then
			for patchfile in ../patches/*; do
				[ -f "$patchfile" ] || continue
				patch -p1 < "$patchfile"
			done
		fi
	)
fi

# Older fetch.sh versions removed every testsuite directory. Restore the
# source directories needed for normal binutils builds from the archive.
restore_testsuite()
{
	subdir=$1
	required_file=$2

	if [ ! -f "dist/$subdir/$required_file" ]; then
		fetch_archive
		mkdir -p "dist/$subdir"
		tar -oxjf binutils-2.34.tar.bz2 -C dist --strip-components=1 \
		    "binutils-2.34/$subdir"
	fi

	if [ ! -f "dist/$subdir/$required_file" ]; then
		echo "binutils fetch: required source dist/$subdir/$required_file is missing" >&2
		exit 1
	fi
}

# Gold's configure and default build both recurse into its testsuite.
restore_testsuite libiberty/testsuite Makefile.in
restore_testsuite binutils/testsuite gentestdlls.c
restore_testsuite gold/testsuite Makefile.in

exit 0
