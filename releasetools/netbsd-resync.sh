#!/bin/sh
: ${BUILDSH=build.sh}

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

if [ -z "${NETBSD_BRANCH}" ]
then
	echo "NETBSD_BRANCH is undefined."
	exit 1
fi

find . -type f    | cut -c 3-   | grep -v '\.git' | grep -v '\./minix' | sort -u > files.all
git grep -i minix | cut -d: -f1 | grep -v '\.git' | grep -v '\./minix' | sort -u > files.minix
diff files.all files.minix |grep '^<'| cut -c 3- > files.netbsd

while read file
do
    git checkout ${NETBSD_BRANCH} ${file}
done < files.netbsd

