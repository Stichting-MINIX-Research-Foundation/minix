#!/bin/sh
# nfs2netbsd - arrange bits of (FreeBSD's) newnfs code for importing
# usage: nfs2netbsd FREEBSDSYSDIR
#
# Caution: unlike most of the *2netbsd scripts in the tree, this copies
# from another dir (which should be the sys/ dir from a FreeBSD checkout)
# rather than operating on a tree already in the current directory.
#
# The current directory should be empty.
#
# $NetBSD: nfs2netbsd.sh,v 1.1 2013/09/30 07:23:37 dholland Exp $

FTOP="$1"

if [ $# != 1 ]; then
    echo "$0: usage: $0 freebsdsysdir" 1>&2
    exit 1
fi

if [ ! -d "$FTOP" ]; then
    echo "$0: $FTOP: not found" 1>&2
    exit 1
fi

############################################################
# 1. Get the list of files.

# Note that we don't (for now anyway) take rpc/* and xdr/*.

FILES=$(egrep -w 'nfscl|nfsd' "$FTOP"/conf/files | awk '{ print $1 }' |\
	sed '/^rpc\//d;/^xdr\//d')

DIRS=$(echo "$FILES" | sed 's,/[^/*]*$,,' | sort -u)

MOREFILES=$(cd "$FTOP" && find $DIRS -name '*.h' -print)

FILES="$FILES $MOREFILES"

############################################################
# 2. Create the directories to copy into.

ALLDIRS=$(echo "$DIRS" | awk -F/ '
    {
	path = sep = "";
	for (i=1;i<=NF;i++) {
	    path = path sep $i;
	    sep = "/";
	    print path;
	}
    }
' | sort -u)

for D in $ALLDIRS; do
    echo "MKDIR   $D"
    mkdir "$D" || exit 1
done

############################################################
# 3. Copy the files.

# In the course of copying, strip the dollar-signs from FreeBSD RCS
# tags and add a NetBSD tag.

for F in $FILES; do
    echo "COPY    $F"
    awk < "$FTOP"/"$F" '
	function detag() {
	    gsub("\\$", "", $0);
	}
	function commentout() {
	    $0 = "/* " $0 " */";
	}
	BEGIN {
	    first = 1;
	}

	# there are a handful of netbsd __RCSID()s in the input
	/__RCSID(.*NetBSD:.*)/ {
	    detag();
	    commentout();
	    print;
	    first = 0;
	    next;
	}
	/__FBSDID(.*FreeBSD:.*)/ {
	    detag();
	    commentout();
	    print;
	    printf "__RCSID(\"%sNetBSD%s\");\n", "$", "$";
	    first = 0;
	    next;
	}
	/\$NetBSD.*\$/ {
	    detag();
	    print;
	    first = 0;
	    next;
	}
	/\$FreeBSD.*\$/ {
	    orig = $0;
	    detag();
	    print;
	    sub("FreeBSD:.*\\$", "NetBSD$", orig);
	    print orig;
	    first = 0;
	    next;
	}
	first {
	    printf "/*\t%sNetBSD%s\t*/\n", "$", "$";
	    print;
	    first = 0;
	    next;
	}
	{ print; }
    ' "name=$F" > "$F"
done

# If you need to diff the files against the freebsd tree for some
# reason, e.g. because you needed to debug the awk script above,
# uncomment this for testing.
#exit 3

############################################################
# 4. Move the files around the way we want them.

# Be sure to reflect changes in this section into section 5.


# If these fail, it means the script needs to be updated...
mv nfs/nfsproto.h nfs/oldnfsproto.h || exit 1
mv nfs/xdr_subs.h nfs/old_xdr_subs.h || exit 1

# Make sure nothing in nfs/ and fs/nfs/ overlaps as we're going
# to merge those dirs.

BAD=$( (
	(cd nfs && ls)
	(cd fs/nfs && ls)
    ) | sort | uniq -d)
if [ x"$BAD" != x ]; then
    echo "$0: The following files exist in both nfs/ and fs/nfs/:" 1>&2
    echo "$BAD" 1>&2
    echo "$0: Please add logic to fix this before continuing." 1>&2
    exit 1
fi

# Now rearrange the dirs.

mkdir fs/nfs/common || exit 1
mv nfs/*.[ch] fs/nfs/common/ || exit 1
mv fs/nfs/*.[ch] fs/nfs/common/ || exit 1
mv fs/nfsserver fs/nfs/server || exit 1
mv fs/nfsclient fs/nfs/client || exit 1
mv nlm fs/nfs/nlm || exit 1

rmdir nfs || exit 1

############################################################
# 5. Prepare a skeleton files.newnfs.

# This helps make sure that freebsd changes in the file list
# propagate.

echo 'GEN     fs/nfs/files.newnfs'

egrep -w 'nfscl|nfsd' "$FTOP"/conf/files |\
	sed '/^rpc\//d;/^xdr\//d' | sed '
    s,^fs/nfs/,fs/nfs/common/,
    s,^fs/nfsclient/,fs/nfs/client/,
    s,^fs/nfsserver/,fs/nfs/server/,
    s,^nfs/,fs/nfs/common/,
    s,^nlm/,fs/nfs/nlm/,
' | sort | awk '
    BEGIN {
	# fbsd -> nbsd translation table for files.* tokens

	# old nfs implementation
	transtoken["nfsserver"] = "false";
	transtoken["nfsclient"] = "false";

	# new nfs implementation
	transtoken["nfscl"] = "new_nfsclient";
	transtoken["nfsd"] = "new_nfsserver";
	transtoken["nfslockd"] = "new_nfslockd";
	transtoken["nfs_root"] = "new_nfs_boot";
	transtoken["bootp"] = "new_nfs_boot_bootp";

	# other stuff
	transtoken["inet"] = "true";
    }
    {
	file = $1;
	expr = "";
	havetoken = 0;
	for (i=2;i<=NF;i++) {
	    if ($i == "optional") {
		continue;
	    }
	    if ($i == "|") {
		havetoken = 0;
	    }
	    else if (havetoken) {
		expr = expr " &";
		havetoken = 0;
	    }
	    else {
		havetoken = 1;
	    }
	    t = $i;
	    if (transtoken[t]) {
		t = transtoken[t];
	    }
	    expr = expr " " t;
	    seentokens[t] = 1;
	}
	gsub("false \\& [a-zA-Z0-9_]+ \\| ", "", expr);
	gsub("false \\| ", "", expr);
	gsub(" \\& true", "", expr);
	files[++nfiles] = file;
	exprs[file] = expr;
    }

    END {
	# This output is not meant to be perfect; it is meant as a
	# starting point.

	printf "#\t%sNetBSD%s\n", "$", "$";
	printf "\n";

	printf "deffs NEW_NFSCLIENT\n";

	sep = "defflag opt_newnfs.h\t\t\t";
	for (t in seentokens) {
	    if (t == "true" || t == "false" || t == "|" || t == "&") {
		continue;
	    }
	    if (t == "new_nfsclient") {
		continue;
	    }
	    printf "%s%s\n", sep, toupper(t);
	    sep = "\t\t\t\t\t";
	}
	printf "\n";

	for (i=1;i<=nfiles;i++) {
	    printf "file\t%s", files[i];
	    ntabs = 4 - int(length(files[i])/8);
	    if (ntabs < 1) {
		ntabs = 1;
	    }
	    for (j=0; j<ntabs; j++) {
		printf "\t";
	    }
	    printf "%s\n", exprs[files[i]];
	}
    }
' > fs/nfs/files.newnfs

############################################################
# 6. done

mv fs/nfs/* . || exit 1
rmdir fs/nfs fs || exit 1

echo "Now do:"
echo "   cvs -d cvs.netbsd.org:/cvsroot import src/sys/fs/nfs FREEBSD FREEBSD-NNNNNN"
echo "where NNNNNN is the subversion version number."
