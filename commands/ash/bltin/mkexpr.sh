#!/bin/sh
# Copyright 1989 by Kenneth Almquist.  All rights reserved.
#
# This file is part of ash.  Ash is distributed under the terms specified
# by the Ash General Public License.  See the file named LICENSE.

# All calls to awk removed, because Minix bawk is deficient.  (kjb)

if [ $# -ne 2 ]
then
	echo "Usage: $0 <unary_op> <binary_op>" >&2
	exit 1
fi
unary_op="$1"
binary_op="$2"

exec > operators.h
i=0
sed -e '/^[^#]/!d' "$unary_op" "$binary_op" | while read line
do
	set -$- $line
	echo "#define $1 $i"
	i=`expr $i + 1`
done
echo
echo "#define FIRST_BINARY_OP" `sed -e '/^[^#]/!d' "$unary_op" | wc -l`
echo '
#define OP_INT 1		/* arguments to operator are integer */
#define OP_STRING 2		/* arguments to operator are string */
#define OP_FILE 3		/* argument is a file name */

extern char *const unary_op[];
extern char *const binary_op[];
extern const char op_priority[];
extern const char op_argflag[];'

exec > operators.c
echo '/*
 * Operators used in the expr/test command.
 */

#include <stddef.h>
#include "shell.h"
#include "operators.h"

char *const unary_op[] = {'
sed -e '/^[^#]/!d
	s/[ 	][ 	]*/ /g
	s/^[^ ][^ ]* \([^ ][^ ]*\).*/      "\1",/
	' "$unary_op"
echo '      NULL
};

char *const binary_op[] = {'
sed -e '/^[^#]/!d
	s/[ 	][ 	]*/ /g
	s/^[^ ][^ ]* \([^ ][^ ]*\).*/      "\1",/
	' "$binary_op"
echo '      NULL
};

const char op_priority[] = {'
sed -e '/^[^#]/!d
	s/[ 	][ 	]*/ /g
	s/^[^ ][^ ]* [^ ][^ ]* \([^ ][^ ]*\).*/      \1,/
	' "$unary_op" "$binary_op"
echo '};

const char op_argflag[] = {'
sed -e '/^[^#]/!d
	s/[ 	][ 	]*/ /g
	s/^[^ ][^ ]* [^ ][^ ]* [^ ][^ ]*$/& 0/
	s/^[^ ][^ ]* [^ ][^ ]* [^ ][^ ]* \([^ ][^ ]*\)/      \1,/
	' "$unary_op" "$binary_op"
echo '};'
