#!/bin/sh

set -e

find_files_and_lines()
(
	find  ../lib/libc/sys ../lib/libsys -name '*.c' | \
	xargs egrep -n '((_syscall|_taskcall)\([^,][^,]*,[ 	]*|_kernel_call\()[A-Z_][A-Z0-9_]*,[ 	]*&m\)' | \
	cut -d: -f1,2
)

# grep message fields
(
	# find references to _syscall, _taskcall and _kernel_call in the libraries
	pathprev=
	linecallprev=0
	for f in `find_files_and_lines`
	do
		path="`echo $f | cut -d: -f1`"
		linecall="`echo $f | cut -d: -f2`"

		# find the matching message declaration; we can identify fields between message decl and call
		linemsg=`head -n "$linecall" "$path" | egrep -n 'message[ 	][ 	]*m;' | tail -n 1 | cut -d: -f1`
		if [ "x$linemsg" != "x" ]
		then
			# watch out: message may be re-used; start from last call in this case
			[ \( "x$path" != "x$pathprev" \) -o \( "$linemsg" -gt "$linecallprev" \) ] || linemsg="$linecallprev"

			# extract call name
			callname=`head -n "$linecall" "$path" | tail -n 1 | sed 's/.*[ 	(,]\([A-Z_][A-Z0-9_]*\),[ 	]*&m).*/\1/'`
			
			# extract message fields
			linelast="`expr "$linecall" - 1`"
			linecount="`expr "$linelast" - "$linemsg"`"
			
			head -n "$linelast" "$path" | \
			tail -n "$linecount" | \
			tr ' \t' ' ' | \
			egrep '^ *m\.[A-Za-z_][A-Za-z0-9_]* *=' | \
			sed 's/^ *m\.\([A-Za-z_][A-Za-z0-9_]*\) *=.*/IDENT('$callname', \1)/'
		fi
		pathprev="$path"
		linemsgprev="$linemsg"
	done
) | sort | uniq
