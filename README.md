# minix
My personal MINIX branch to support uefi boot.

Develop enviromment
- cross-compile on Ubuntu 24.04

Currently done.
- i386/amd64 support.
- build gpt partitoned live image
  - fetch texinfo and binutils sources
  - run `./build.sh -U -u -mi386|amd64 -O ../build release`
  - run `./build.sh -U -u -mi386|amd64 -O ../build live-image`
  - live disk image at 
   ../build/distrib/i386/liveimage/emuimage/Minix-3.4.0-i386-live.img
   ../build/distrib/amd64/liveimage/emuimage/Minix-3.4.0-x86_64-live.img
- boot to efi boot loader (bootia32.efi|bootx64.efi)
- boot to minix kernel (multiboot2)
- show login prompt
- posix test pass 94/97

Not included (no plan).
- self compile
- MINIX installer for uefi/gpt
- iso-image


