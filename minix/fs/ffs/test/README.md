# FFS (UFS2) host regression test

This directory contains a **host-side** test for the MINIX FFS server
(`minix/fs/ffs`), `newfs_ffs` and `fsck_ffs`.  It exists because the x86_64
MINIX port does not yet boot to a shell, so the server cannot be exercised on
MINIX itself.  Instead, the test compiles the *real, unmodified* server source
files on the build host against a small shim and runs them against an actual
UFS2 image file.

## How it works

`compat/` provides just enough of the MINIX / libminixfs / libfsdriver / libbdev
headers for the server sources to compile on a Linux host:

- `shim.c` implements the lmfs block cache (over a plain image file via
  `pread`/`pwrite`), `bdev_*`, and the `fsdriver_*` copy/getdents helpers.
- `globals.c` allocates the server's `EXTERN` globals (the job `table.c` does in
  the real build).
- `harness.c` drives the VFS-FS handlers directly (`fs_mount`, `fs_create`,
  `fs_readwrite`, `fs_mkdir`, `fs_unlink`, ...) and checks the results.
- `create_only.c` writes content without deleting it (so `fsck_ffs` sees a
  populated, non-trivial file system).
- `ufs2dump.c` is an independent reader (it uses only the on-disk header
  `../ffs_disk.h`) that dumps the superblock, root inode and directory.

`main.c` and `table.c` are **not** compiled (the harness supplies its own
`main` and calls the handlers directly).

## Running

```sh
cd minix/fs/ffs/test
sh run.sh
# If nbmakefs is not on $PATH, point TOOLDIR at the cross tools first:
TOOLDIR=.../build/tooldir.*/bin sh run.sh
```

The script runs four scenarios:

1. `newfs_ffs` creates an image; the server does create / multi-fragment write /
   mkdir / nested file / unlink / rmdir; `fsck_ffs` confirms it is clean and all
   free counts reclaim exactly.
2. The server writes content (no deletes); `fsck_ffs` must stay clean (proves
   the server keeps the file system fsck-consistent after writes).
3. Cross-check: the server reads a reference image produced by `nbmakefs`
   (if available) — note that makefs writes the superblock at offset 8192 with
   the UFS2EA magic, which the server's `SBLOCKSEARCH` handles.
4. Negative test: corrupt `fs_cstotal` and confirm `fsck_ffs` reports it.

In scenario 1 the harness prints "3 failed" — those are its hard-coded checks
for `hello.txt`/`sub`, which only exist on the reference makefs image, not on a
freshly created one.  Every functional check passes.

## Important

This is **not** built by the normal MINIX build (it is host-only and is not in
any `Makefile`).  It is a developer tool; keep it in sync with the server when
the on-disk handling changes.  The shim is a simplification (single device,
device-block-keyed cache, no concurrency) and does not exercise the VM
second-level cache path.
