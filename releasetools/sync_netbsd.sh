#!/bin/sh
# Sync NetBSD sources from upstream
#
# Environment variables:
#   NETBSD_BRANCH - branch or tag to sync (required)
#   NETBSD_REMOTE - name of git remote (default: netbsd)
#   NETBSD_URL    - remote URL (default: https://github.com/NetBSD/src.git)
#
: ${NETBSD_REMOTE:=netbsd}
: ${NETBSD_URL:=https://github.com/NetBSD/src.git}

if [ -z "$NETBSD_BRANCH" ]; then
    echo "Please set NETBSD_BRANCH to the upstream branch or tag." >&2
    exit 1
fi

if [ ! -f build.sh ]; then
    echo "Run this script from the repository root." >&2
    exit 1
fi

set -e

# Add remote if missing
if ! git remote | grep -q "^${NETBSD_REMOTE}$"; then
    git remote add "$NETBSD_REMOTE" "$NETBSD_URL"
fi

# Fetch updates from upstream
git fetch "$NETBSD_REMOTE"

UPDATE_BRANCH="netbsd-sync-${NETBSD_BRANCH}"

git branch -f "$UPDATE_BRANCH" "$NETBSD_REMOTE/$NETBSD_BRANCH"

find . -type f | cut -c 3- | grep -v '^\.git' | grep -v '^./minix' | sort -u > files.all
git grep -l -i minix | grep -v '^\.git' | grep -v '^./minix' | sort -u > files.minix
comm -23 files.all files.minix > files.netbsd

while read -r file; do
    git checkout "$UPDATE_BRANCH" -- "$file"
done < files.netbsd

rm files.all files.minix files.netbsd

echo "NetBSD files synced from $NETBSD_BRANCH"
