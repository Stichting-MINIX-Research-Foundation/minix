#!/bin/sh
#
# makewhatis 2.0 - make whatis(5) database.	Author: Kees J. Bot.
#
# Make the whatis database of a man directory from the manual pages.

case $1 in
-*)	set -$- x x
esac

case $# in
1)	;;
*)	echo "Usage: $0 <mandir>" >&2
	exit 1
esac

cd $1 || exit

{
	# First pass, gathering the .SH NAME lines in various forms.

	# First the man[1-8] directories, the titles are under the .SH NAME
	# section header.
	for chap in 1 2 3 4 5 6 7 8
	do
		for page in man$chap/*.$chap
		do
		   if	test -f "$page"; then	# (Old sh barfs on 'continue')

			sed -e 's/	/ /g
				/^\.SH NAME/,/^\.SH /!d
				/^\.SH /d
				s/\\f.//g	# should not be needed
				s/\\s[+-].//g
				s/\\s.//g
				s/\\//
				'"s/ - / ($chap) - /" < "$page"
		   fi
		done
	done

	# The Minix "Book style" documents, look for .CD
	for page in man9/*.9
	do
	   if	test -f "$page"; then

		sed -e 's/	/ /g
			/^\.CD /!d
			s/^[^"]*"//
			s/"[^"]*$//
			s/\\(en/-/g
			s/\\f.//g
			s/\\s[+-].//g
			s/\\s.//g
			s/\\\*(M2/MINIX/g
			s/\\//
			'"s/ - / (9) - /" < "$page"
	   fi
	done

	# Some people throw extra flat text files into the cat[1-9]
	# directories.  It would be nice if man(1) can find them.
	trap 'rm -f /tmp/mkw[cmn]$$; exit 1' 1 2 15
	for chap in 1 2 3 4 5 6 7 8 9
	do
		ls cat$chap 2>/dev/null >/tmp/mkwc$$
		ls man$chap 2>/dev/null >/tmp/mkwm$$
		comm -23 /tmp/mkwc$$ /tmp/mkwm$$ >/tmp/mkwn$$
		sed -e "/.*\\.$chap\$/!d
			s/\\.$chap\$/ ($chap) - ???/" < /tmp/mkwn$$
	done
	rm -f /tmp/mkw[cmn]$$
} | {
	# Second pass, remove empty lines, leading and trailing spaces,
	# multiple spaces to one space, remove lines without a dash.
	sed -e 's/  */ /g
		s/^ //
		s/ $//
		/^$/d
		/-/!d'
} | {
	# Third pass, sort by section.
	sort -t'(' +1 -o whatis
}
