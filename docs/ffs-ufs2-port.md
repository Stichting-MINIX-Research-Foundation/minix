# FFS / UFS2 Filesystem Port

## Background

MINIX had no support for the Berkeley Fast File System (FFS / UFS). This work
ports the NetBSD FFS **on-disk format** (UFS2 variant) to MINIX as a new
userspace file-system server, `minix/fs/ffs`, plus the userland tools
`newfs_ffs` (create) and `fsck_ffs` (check).

The NetBSD kernel code in `sys/ufs/ffs` cannot be reused directly: it is bound
to the BSD VFS (`struct vnode`, `vfsops`, `struct buf`, kernel locking, WAPBL
journaling, snapshots, quotas, dirhash, extended attributes). A MINIX FS server
instead speaks the message-based VFS-FS protocol through **libfsdriver**
(`fsdriver_task`) and caches blocks through **libminixfs** (`lmfs_*`), exactly
like the existing `minix/fs/ext2` server — which is itself a from-scratch
reimplementation of the ext2 *on-disk format*, not a port of `sys/ufs/ext2fs`.

So this port keeps the FFS/UFS **on-disk format and the pure allocation
algorithms**, and replaces all BSD VFS plumbing with the MINIX fsdriver + lmfs
framework, modeled file-for-file on `minix/fs/ext2`.

Scope: **UFS2 and UFS1 (FFSv1), native little-endian, read-write.** UFS1 is
supported by keeping a widened UFS2 dinode in core and converting to/from the
narrower on-disk UFS1 dinode at the inode-I/O boundary (see "UFS1 (FFSv1)
support" below).

## Components

| Path | Role |
|------|------|
| `minix/fs/ffs/` | The file-system server (~4,500 lines, 26 files) |
| `minix/sbin/newfs_ffs/` | Create a UFS2 file system (~570 lines) |
| `minix/sbin/fsck_ffs/` | Read-only consistency checker (~220 lines) |
| `minix/fs/ffs/test/` | Host-side regression harness (developer tool, not built) |

Build/sets wiring: `minix/fs/Makefile` (`SUBDIR+=ffs`),
`minix/sbin/Makefile` (`SUBDIR+=newfs_ffs fsck_ffs`), and
`distrib/sets/lists/minix-base/mi` (`./service/ffs`, `./sbin/newfs_ffs`,
`./sbin/fsck_ffs`).

`mount -t ffs <dev> <dir>` works with **no `mount_ffs` binary**: MINIX libc
`minix_mount()` execs `/service/<type>`, so installing `/service/ffs` is the
entire mount mechanism.

Type **auto-detection** (`mount <dev> <dir>` with no `-t`, and `df`/other
`fsversion()` callers) also recognises UFS2. `fsversion()`
(`minix/lib/libc/gen/fsversion.c`) probes the superblock candidate offsets
65536 (standard `newfs`) and 8192 (`nbmakefs -o version=2`, and the UFS1
superblock), reads `fs_magic` at struct-offset 1372, accepts `FS_UFS2_MAGIC`
(0x19540119), `FS_UFS2EA_MAGIC` (0x19012038) and `FS_UFS1_MAGIC` (0x011954),
and confirms `fs_sblockloc` (offset 1000) equals the probe offset to reject
false positives. It returns the new `FSVERSION_FFS`
(`minix/include/minix/minlib.h`), which `minix/commands/mount/mount.c` maps to
type `ffs`. Without this, an unflagged mount of a UFS volume fell through to
the `mfs` default.

## On-disk format

The on-disk definitions are vendored, trimmed to UFS1+UFS2 / native-LE, into
`minix/fs/ffs/ffs_disk.h` (from `sys/ufs/ffs/fs.h`, `sys/ufs/ufs/dinode.h`,
`sys/ufs/ufs/dir.h`). The layout was verified to match NetBSD/amd64 (LP64):

| Structure | Size / offset |
|-----------|---------------|
| `struct fs` | 1376 bytes; `fs_magic` @ 1372; `fs_sblockloc` @ 1000; `fs_cstotal` @ 1008; UFS1 legacy in `fs_old_*` |
| `struct ufs2_dinode` | 256 bytes; `di_db` @ 112; `di_ib` @ 208 |
| `struct ufs1_dinode` | 128 bytes; 32-bit `di_db` @ 40, `di_ib` @ 88 |
| `struct direct` | variable; entries never cross a 512-byte (`UFS_DIRBLKSIZ`) boundary |

The in-core inode always holds a `struct ufs2_dinode` (`rip->i_din.di_*`); no
field-by-field byte swapping is needed because the server is native-LE. On a
UFS1 file system the narrower on-disk dinode is converted to/from this form at
the `rw_inode` boundary (see "UFS1 (FFSv1) support").

## Central design decision: fragments vs. a fixed cache block size

`lmfs` uses a single fixed block size per mount, but UFS splits its large block
(`fs_bsize`) into `fs_frag` fragments (`fs_fsize`); the tail block of a file and
all cylinder-group bitmaps are fragment-granular.

The server sets the lmfs cache block size to the **fragment** size:

```c
lmfs_set_blocksize(fs->fs_fsize);
```

Consequences:

- All on-disk addresses (`di_db[]`, `di_ib[]`, indirect-block entries, cg
  bitmaps) are fragment numbers that map **1:1** onto lmfs cache block numbers
  (`block64_t`).
- A full file-system block occupies `fs_frag` consecutive cache blocks.
- An indirect block (`fs_bsize`) is read one fragment at a time
  (`read.c:indir_get`, `write.c:read_indir_entry`).
- Fragments occur **only in the direct-block range**: `ffs_blksize()` returns a
  full `bsize` for every `lbn >= UFS_NDADDR`, so indirect-addressed data blocks
  and all metadata blocks are always whole blocks. This is what keeps
  `ffs_balloc` tractable.

The VM second-level (inode-keyed) cache is **disabled**
(`lmfs_may_use_vmcache(0)`): fragment relocation in `ffs_realloccg` moves data
between device blocks, and device-block-keyed caching keeps that coherent
without per-block VM invalidation.

## Allocator

`balloc.c` is a self-contained port of the on-disk semantics of NetBSD's
`ffs_alloc.c`. Cylinder groups are read into and written back from a contiguous
`fs_bsize` bounce buffer assembled out of the `fs_frag` cache blocks
(`ffs_read_cg` / `ffs_write_cg`). Fragment accounting (`cg_frsum`, `cg_cs`,
`fs_cstotal`, `fs_csp[]`) is maintained exactly as the original FFS, using the
`fragtbl` / `around` / `inside` tables and `ffs_fragacct`, so images stay
fsck-clean. `ialloc.c` ports inode allocation including the UFS2 lazy inode-block
initialization (`cg_initediblk`).

One subtle, important predicate:

```c
/* ffs_isblock(cp,h)     -> cp[h]==0xff : block is already FREE (allocatable) */
/* ffs_isfreeblock(cp,h) -> cp[h]==0    : block is fully ALLOCATED            */
```

The whole-block double-free guard in `ffs_blkfree` must use **`ffs_isblock`**
(already-free). Using `ffs_isfreeblock` there inverts the test and silently
skips the free of every directory's block — a leak that the host harness caught.

## Superblock location

`read_super` performs the standard `SBLOCKSEARCH` (65536 → 8192 → 0 → 256K) and
accepts **both** `FS_UFS2_MAGIC` (0x19540119) and `FS_UFS2EA_MAGIC`
(0x19012038), validating `fs_sblockloc == offset`; `write_super` writes back to
the discovered offset.

Real-world gotcha discovered by testing actual images:

| Tool | Superblock offset | Magic |
|------|-------------------|-------|
| `nbmakefs -t ffs -o version=1` | **8192** | UFS1 (0x011954) |
| `nbmakefs -t ffs -o version=2` | **8192** | UFS2EA (0x19012038) |
| standard `newfs` / our `newfs_ffs` | 65536 | UFS2 (0x19540119) |

The server therefore must not hardcode the offset or a single magic.

## UFS1 (FFSv1) support

UFS1 differs from UFS2 in three ways the server has to bridge; everything else
(cylinder-group layout, bitmaps, `struct csum` per-cg summary, the new-format
directory with `d_type`) is identical for a modern (`FS_44INODEFMT`) file
system as written by `newfs`/`makefs`.

1. **Dinode (128 B vs 256 B, 32-bit block pointers).** The in-core inode always
   holds a *wide* `struct ufs2_dinode`; `rw_inode` (`inode.c`) widens the on-disk
   `struct ufs1_dinode` on read (`ufs1_to_ufs2`) and narrows it back on write
   (`ufs2_to_ufs1`), so the entire rest of the server stays UFS2-shaped. Block
   numbers are small non-negative frag numbers, so the widen is a loss-free
   sign-extend. **Inline ("fast") symlinks are the exception**: their target
   bytes live in the block-pointer area (`di_db`/`di_ib`), so they are copied
   verbatim (`UFS1_MAXSYMLINKLEN` = 60 bytes) — a numeric widen/narrow of those
   bytes would corrupt the string. The stride within an inode block is
   `DINODE1_SIZE` (128), not `DINODE2_SIZE`.

2. **Superblock legacy fields.** UFS1 keeps `size`, `dsize`, the cg-summary
   address, the free-count totals and the last-written time in narrow
   `fs_old_*` slots. `read_super` normalizes them up into the wide UFS2 fields
   (`ffs_oldfscompat_read`) and `write_super` copies them back down
   (`ffs_oldfscompat_write`); `fs_qbmask`/`fs_qfmask`/`fs_maxfilesize` are
   recomputed rather than trusted. After this the allocator, `mount.c`
   free-space math and `fsck_ffs` all read only the wide fields. The wide
   `fs_magic` is preserved on disk, so `fs->fs_magic == FS_UFS1_MAGIC` is the
   single format discriminator used throughout.

3. **32-bit indirect-block entries.** Indirect blocks hold `int32_t` addresses
   on UFS1, `int64_t` on UFS2. The three centralized accessors (`read.c`
   `indir_get`, `write.c` `read_indir_entry`/`write_indir_entry`) use
   `FFS_DADDRSIZE(fs)` for the entry stride and the width-aware
   `ffs_getdaddr`/`ffs_putdaddr` helpers (`ffs_disk.h`), which covers the read,
   write and truncate paths in one place.

One UFS2-only step is **skipped** on UFS1: the lazy inode-block initialization
(`cg_initediblk`) in `ialloc.c`. UFS1 initializes every inode block at newfs
time and has no `cg_initediblk` counter (the field is garbage), so running the
UFS2 loop would zero live inode blocks — the whole loop is guarded on
`fs_magic != FS_UFS1_MAGIC`.

`newfs_ffs` still creates only UFS2 images; UFS1 images come from `newfs -O1`
or `nbmakefs -o version=1`. `fsck_ffs` handles both (it normalizes the
`fs_old_*` fields and uses the right inode stride; `di_mode`/`di_nlink` sit at
the same offset in both dinode formats).

## newfs_ffs

A focused port of the geometry / superblock / cylinder-group logic from
`usr.sbin/makefs/ffs/mkfs.c`, restricted to native-LE UFS2 with no rotational /
cluster optimization (`fs_contigsumsize == 0`). Unlike makefs it writes directly
to the device and creates the root directory itself (makefs delegates directory
creation to a higher layer). It writes a standard superblock at 65536 with the
plain UFS2 magic.

## fsck_ffs

A **read-only** checker (like `fsck -n`, no repair): validates superblock
geometry, then for every cylinder group recomputes free-block / free-fragment /
free-inode counts directly from the bitmaps and cross-checks them against
`cg_cs`, the `fs_cs` summary array, and `fs_cstotal`; it also sanity-checks the
root inode. Exit code 4 on inconsistency. A full pass1–4 repairing fsck is left
as future work.

## Validation

The server now mounts read/write on a running MINIX/x86_64 (`mount -t ffs`, with
the new `fsversion()` UFS2 auto-detection); it is additionally validated on the
build host: `minix/fs/ffs/test/` compiles the **real, unmodified server source
files** on the build host against a small lmfs / bdev / fsdriver shim
(`compat/`, `shim.c`, `globals.c`) and runs the actual VFS-FS handlers against a
real UFS2 image file. Run with `sh run.sh`.

The test cross-validates the whole stack:

1. `newfs_ffs` creates an image → the server does create / multi-fragment write /
   mkdir / nested file / unlink / rmdir → free blocks and inodes reclaim exactly
   → `fsck_ffs` reports clean.
1b. Indirect blocks beyond the 12 direct (`bigfile`: a 2 MiB file spanning
   direct + single indirect, with boundary reads and full-tree truncate;
   `sparse`: high-offset sparse writes that reach the double- and triple-indirect
   allocation/free recursion; holes read as zero; blocks fully reclaim).  Fast
   (inline) and slow (data-block) symlinks (`slink`).  `ftruncate`-grow edge
   cases including fragment enlargement and frag→block rounding (`truncgrow`),
   each followed by `fsck_ffs`.
2. The server writes content (no deletes) → `fsck_ffs` stays clean (the server
   keeps the file system fsck-consistent).
3. The server reads a reference `nbmakefs` image (the 8192 / UFS2EA case).
3b. **UFS1 (FFSv1):** on empty `nbmakefs -o version=1` images the server runs
   the full write/delete harness, `bigfile` (direct + single indirect, 2 MiB,
   truncate), `sparse` (single/double/triple indirect, holes, full-tree free)
   and `slink` (fast + slow symlinks), each followed by `fsck_ffs`. This
   exercises the UFS1 dinode conversion, the 32-bit indirect entries and the
   old↔new superblock field sync, and proves the write path keeps a UFS1 image
   fsck-consistent (free blocks and inodes reclaim exactly).
4. Negative test: corrupting `fs_cstotal` is detected by `fsck_ffs`.

Read-path internals were additionally checked with an independent reader
(`test/ufs2dump.c`) that uses only the on-disk header.

## Bugs found and fixed during the port

- Two directory link-count double-decrements (rmdir, cross-directory rename) —
  `unlink_file("..")` already adjusts the parent.
- Stale data exposed when a tail fragment is grown to a full block — the grown
  region must be zeroed (`write.c:zero_block_range`).
- 64-bit `lastblock` truncated to `int` in `truncate_inode`.
- Superblock `s_csp` mmap leaked on mount failure / across mounts.
- The inverted `ffs_isblock` / `ffs_isfreeblock` predicate in `ffs_blkfree`
  (directory-block leak) — found by the host harness.
- `truncate_inode` grow path advanced `di_size` past the EOF fragment without
  enlarging it: an `ftruncate`-grow within a direct block (or one that only
  extended the trailing fragment) left the on-disk fragment smaller than
  `fragroundup(blkoff(di_size))`.  The next grow of that block derived the
  "old size" from `di_size`, so `ffs_realloccg`/`ffs_fragextend` mis-counted the
  fragment delta and `cg_cs.cs_nffree` drifted from the bitmap (fsck:
  "free-frag count N != bitmap M").  Fix: a growing truncate now sizes the new
  EOF block via `ffs_balloc(rip, length - 1, 1)` when the EOF is in the direct
  range (which also rounds an earlier partial direct block up to a full block
  and zeroes the grown bytes), keeping the fragment consistent with `di_size`.
  Found by the new `truncgrow` host test.

## Limitations / future work

- UFS1 and UFS2 read-write, but only **modern** (`FS_44INODEFMT`, new-format cg
  and directories) native-little-endian images; no opposite-endian images, no
  pre-4.4BSD UFS1 formats, no extended attributes, journaling (WAPBL),
  snapshots, quotas, or dirhash.
- `newfs_ffs` creates UFS2 only; make UFS1 images with `newfs -O1` /
  `nbmakefs -o version=1`. UFS1's 32-bit time fields are subject to the 2038
  rollover.
- New directories are allocated a full block (simple and fsck-clean, but uses
  more space than a fragment-sized directory).
- `fsck_ffs` checks but does not repair.
- No crash consistency / ordering guarantees (no soft updates or journaling) —
  like MINIX MFS/ext2, an unclean shutdown can leave the file system needing an
  fsck.
