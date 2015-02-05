#!/bin/sh 
#
# Perform a checkout / update the MINIX u-boot git repo if needed
# 
: ${UBOOT_REPO_URL=git://git.minix3.org/u-boot}

# -o output dir
OUTPUT_DIR=""
GIT_VERSION=""
while getopts "o:n:?" c
do
        case "$c" in
        \?)
                echo "Usage: $0 -o output dir -n version " >&2
                exit 1
        	;;
        o)
                OUTPUT_DIR=$OPTARG
		;;
        n)
                GIT_VERSION=$OPTARG
		;;
	esac
done


#
# check arguments
#
if [ -z "$OUTPUT_DIR" -o -z "$GIT_VERSION" ]
then
		echo "Missing required parameters OUTPUT_DIR=$OUTPUT_DIR GIT_VERSION=$GIT_VERSION"
                echo "Usage: $0 -o output dir -n version " >&2
                exit 1
fi


#
# if the file doesn't exist it's easy , to a checkout 
#
if  [ ! -e "$OUTPUT_DIR" ]
then
	git clone ${UBOOT_REPO_URL} -b minix $OUTPUT_DIR
fi

(
	cd  "$OUTPUT_DIR"

	#
	# perform an update
	#
	CURRENT_VERSION=`git rev-parse HEAD`
	if [ "$CURRENT_VERSION" !=  "$GIT_VERSION" ]
	then
		echo "Current version $CURRENT_VERSION does not match wanted $GIT_VERSION performing update and checkout"	
		git fetch -v 
		git checkout $GIT_VERSION
	fi
)
