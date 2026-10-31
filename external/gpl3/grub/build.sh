#!/bin/sh

./bootstrap
./configure --with-platform=efi --target=i386
make
cd grub-core
../grub-mkimage -v -d . -o booti386.efi -O i386-efi -p /EFI/BOOT \
	normal serial part_msdos part_gpt fat chain boot configfile \
	multiboot multiboot2 minix3 gzio efi_uga all_video
ls -l booti386.efi

