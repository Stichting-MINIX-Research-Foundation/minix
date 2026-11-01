# docker/ — containerized development for this tree

One container image (`minix-dev`, see `Dockerfile`) carries every host-side
dependency: NetBSD `build.sh`, the `releasetools` image build, the
`../minix-ports` cross-toolchain build, and the headless QEMU harness.

## Why the extra obj dir?

Docker images (Debian 12 / glibc 2.36) cannot run binaries built against the
host's newer glibc. The reverse works. Rule: **build inside the container,
run artifacts everywhere**. Consequently:

- OS objects: `../obj.docker.i386` (the canonical objdir; the old host
  `../obj.i386` was retired):
  ```
  docker/dev.sh sh build.sh -m i386 -O ../obj.docker.i386 -U -u tools
  docker/dev.sh env OBJ=../obj.docker.i386 JOBS=$(nproc) ./releasetools/x86_hdimage.sh
  ```
- The cross toolchain (`../minix-ports/build-toolchain.sh`) defaults its
  sysroot to `../obj.docker.i386/destdir.i386` (override with MINIX_SYSROOT).

## Quick start

```
docker/dev.sh image                                  # build image (once)
docker/dev.sh shell                                  # get a shell
docker/dev.sh ./releasetools/x86_qemu.py --cmd 'uname -a'   # harness smoke
docker/dev.sh ../minix-ports/test-toolchain.sh       # toolchain gate in-guest
```

KVM is passed through automatically when `/dev/kvm` is accessible; otherwise
QEMU falls back to TCG (slower, still works).
