#!/bin/sh
#
# Perform a checkout / update of git repo if needed
#

OUTPUT_DIR=""
GIT_REVISION=""
REPO_URL=""
BRANCH="master"

while getopts "o:n:b:?" c
do
	case "$c" in
	\?)
		echo "Usage: $0 [-b branch] [-o output_dir] -n revision repo_url" >&2
		exit 1
		;;
	o)
		OUTPUT_DIR=$OPTARG
		;;
	n)
		GIT_REVISION=$OPTARG
		;;
	b)
		BRANCH=$OPTARG
		;;
	esac
done

shift $(($OPTIND - 1))

REPO_URL=$1

#
# check arguments
#
if [ -z "$GIT_REVISION" -o -z "$REPO_URL" -o -z "$OUTPUT_DIR" ]
then
	echo "Usage: $0 [-b branch] -o output_dir -n revision repo_url" >&2
	exit 1
fi

#
# if the file doesn't exist it's easy , to a checkout
#
if  [ ! -e "$OUTPUT_DIR" ]
then
	git clone ${REPO_URL} -b $BRANCH $OUTPUT_DIR
fi

(
	cd "$OUTPUT_DIR"

	#
	# perform an update
	#
	CURRENT_VERSION=$(git rev-parse HEAD)

	echo "Checking out $REPO_URL..."
	echo " * Current revision: $CURRENT_VERSION"
	echo " * Wanted revision:  $GIT_REVISION"

	if [ "$CURRENT_VERSION" != "$GIT_REVISION" ]
	then
		echo " * Performing update and checkout..."
		git fetch -v
		git checkout $GIT_REVISION
	fi
)
