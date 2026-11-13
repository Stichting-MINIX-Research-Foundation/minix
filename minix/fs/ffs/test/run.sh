#!/bin/sh
# Host-side regression test for the MINIX FFS (UFS2) server, newfs_ffs and
# fsck_ffs.  It compiles the *real* server source files against a small
# lmfs/bdev/fsdriver shim (see compat/ and shim.c) and runs them against a real
# UFS2 image on the build host -- no MINIX boot required.
#
# Requires: a C compiler, and (optionally) the cross tool nbmakefs to create a
# reference UFS2 image.  Point TOOLDIR at .../build/tooldir.*/bin if nbmakefs is
# not already on $PATH.
#
# Usage: cd minix/fs/ffs/test && sh run.sh
set -e

here=$(cd "$(dirname "$0")" && pwd)
srv="$here/.."				# the server source directory
sbin="$here/../../../sbin"		# newfs_ffs / fsck_ffs
work=$(mktemp -d)
cc=${CC:-cc}
CFLAGS="-D_GNU_SOURCE -w"

# server objects to link (everything except main.c and table.c)
SRV_SRCS="super mount inode read path stadir balloc ialloc write open link \
	protect time misc utility"
srv_list=""
for s in $SRV_SRCS; do srv_list="$srv_list $srv/$s.c"; done

echo "== building host test binaries in $work =="
$cc $CFLAGS -I"$here/compat" -I"$srv" -o "$work/harness" \
	"$here/globals.c" "$here/shim.c" "$here/harness.c" $srv_list
$cc $CFLAGS -I"$here/compat" -I"$srv" -o "$work/create_only" \
	"$here/globals.c" "$here/shim.c" "$here/create_only.c" $srv_list
for t in bigfile sparse slink truncgrow; do
	$cc $CFLAGS -I"$here/compat" -I"$srv" -o "$work/$t" \
		"$here/globals.c" "$here/shim.c" "$here/$t.c" $srv_list
done
$cc $CFLAGS -I"$srv" -o "$work/ufs2dump" "$here/ufs2dump.c"
$cc $CFLAGS -I"$sbin/newfs_ffs" -o "$work/newfs_ffs" "$sbin/newfs_ffs/newfs_ffs.c"
$cc $CFLAGS -I"$sbin/fsck_ffs"  -o "$work/fsck_ffs"  "$sbin/fsck_ffs/fsck_ffs.c"

# locate nbmakefs (optional; used for a cross-check against reference images)
nbmakefs=$(command -v nbmakefs || true)
if [ -z "$nbmakefs" ] && [ -n "$TOOLDIR" ]; then
	nbmakefs="$TOOLDIR/nbmakefs"
fi

echo
echo "== 1. newfs_ffs creates an image, server read/writes it, fsck checks it =="
"$work/newfs_ffs" -s 16384 "$work/a.img" >/dev/null
"$work/harness" "$work/a.img"
echo "-- fsck after full harness run --"
"$work/fsck_ffs" "$work/a.img"

echo
echo "== 1b. indirect blocks (direct+single/double/triple), symlinks, ftruncate-grow =="
"$work/newfs_ffs" -s 131072 "$work/big.img" >/dev/null
"$work/bigfile" "$work/big.img"; "$work/fsck_ffs" "$work/big.img"
"$work/newfs_ffs" -s 131072 "$work/sp.img" >/dev/null
"$work/sparse" "$work/sp.img"; "$work/fsck_ffs" "$work/sp.img"
"$work/newfs_ffs" -s 16384 "$work/sl.img" >/dev/null
"$work/slink" "$work/sl.img"; "$work/fsck_ffs" "$work/sl.img"
"$work/newfs_ffs" -s 16384 "$work/tg.img" >/dev/null
"$work/truncgrow" "$work/tg.img"; "$work/fsck_ffs" "$work/tg.img"

echo
echo "== 2. server writes content (no deletes), fsck must stay clean =="
"$work/newfs_ffs" -s 16384 "$work/b.img" >/dev/null
"$work/create_only" "$work/b.img"
"$work/ufs2dump" "$work/b.img" | sed -n '/directory entries/,$p'
"$work/fsck_ffs" "$work/b.img"

if [ -n "$nbmakefs" ] && [ -x "$nbmakefs" ]; then
	echo
	echo "== 3. cross-check: server reads a reference makefs UFS2 image =="
	rm -rf "$work/seed"; mkdir -p "$work/seed/sub"
	echo "hello ufs2" > "$work/seed/hello.txt"
	ln -s hello.txt "$work/seed/link.txt"
	"$nbmakefs" -t ffs -o version=2,bsize=32768,fsize=4096 -s 8m \
		"$work/c.img" "$work/seed" >/dev/null
	"$work/ufs2dump" "$work/c.img" | sed -n '/directory entries/,$p'
	"$work/fsck_ffs" "$work/c.img"

	echo
	echo "== 3b. UFS1 (FFSv1): server mounts makefs version=1 images r/w =="
	# Build empty UFS1 file systems with makefs and run the real handlers on
	# them.  This exercises the UFS1 dinode conversion, the old<->new
	# superblock field sync, and -- via bigfile/sparse -- the 32-bit indirect
	# block entries.  Each run is followed by fsck to prove the write path
	# keeps a UFS1 image fsck-consistent.
	rm -rf "$work/empty"; mkdir -p "$work/empty"
	mku1() { "$nbmakefs" -t ffs -o version=1,bsize=16384,fsize=2048 -s 32m \
		"$1" "$work/empty" >/dev/null; }

	mku1 "$work/u1.img";  "$work/fsck_ffs" "$work/u1.img"
	"$work/harness" "$work/u1.img";  "$work/fsck_ffs" "$work/u1.img"
	echo "-- UFS1 indirect blocks (direct+single) --"
	mku1 "$work/u1big.img"
	"$work/bigfile" "$work/u1big.img";  "$work/fsck_ffs" "$work/u1big.img"
	echo "-- UFS1 sparse (double/triple indirect) --"
	mku1 "$work/u1sp.img"
	"$work/sparse" "$work/u1sp.img";  "$work/fsck_ffs" "$work/u1sp.img"
	echo "-- UFS1 symlinks (fast+slow) --"
	mku1 "$work/u1sl.img"
	"$work/slink" "$work/u1sl.img";  "$work/fsck_ffs" "$work/u1sl.img"

	echo
	echo "== 3c. UFS2 fs_frag=4: fragment allocation on makefs images =="
	# Regression test for the ffs_mapsearch byte-scan mask: with
	# bsize=16384/fsize=4096 (fs_frag == 4, the live-image geometry), the
	# fragtbl availability bits start at bit fs_frag, not bit 0.  The old
	# bit-(allocsiz-1) mask made every fragment allocation return ENOSPC
	# on such images while fs_frag == 8 images (all the other tests here)
	# worked fine.
	"$nbmakefs" -t ffs -o version=2,bsize=16384,fsize=4096 -s 32m \
		"$work/u2f4.img" "$work/empty" >/dev/null
	"$work/harness" "$work/u2f4.img";  "$work/fsck_ffs" "$work/u2f4.img"
fi

echo
echo "== 4. negative test: corrupt fs_cstotal, fsck must report it =="
cp "$work/a.img" "$work/bad.img"
python3 - "$work/bad.img" <<'PY'
import struct, sys
f = open(sys.argv[1], 'r+b')
off = 65536 + 1008 + 8		# fs_cstotal.cs_nbfree
f.seek(off); v = struct.unpack('<q', f.read(8))[0]
f.seek(off); f.write(struct.pack('<q', v + 5))
f.close()
PY
if "$work/fsck_ffs" "$work/bad.img"; then
	echo "FAIL: fsck did not detect corruption"; rm -rf "$work"; exit 1
else
	echo "OK: fsck detected the injected corruption"
fi

rm -rf "$work"
echo
echo "== all FFS host tests passed =="
