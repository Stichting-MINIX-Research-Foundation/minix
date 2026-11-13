# EFI64 early-boot trace

The x86_64 Multiboot2 bootstrap writes progress markers to COM1 (115200 baud)
before the normal MINIX console and IDT are installed. These messages are
deliberately limited to the early bootstrap path and are useful when a UEFI
firmware reset hides the original exception.

Rebuild the amd64 kernel and installation ISO, then run QEMU with the serial
port connected and automatic reboot disabled:

```sh
qemu-system-x86_64 \
  -m 1024 \
  -bios /usr/share/ovmf/OVMF.fd \
  -cdrom /home/user/build/releasedir/images/Minix-3.4.0-x86_64-install.iso \
  -boot d \
  -serial stdio \
  -no-reboot \
  -d int,cpu_reset \
  -D /tmp/minix-efi64-qemu.log
```

The serial output identifies the last completed stage:

- No `pre_init entered`: execution did not reach the C bootstrap (check the
  EFI64 entry address, long-mode state, and initial stack).
- `pre_init entered` but no `get_parameters_mb2 entered`: failure while
  setting up the early serial path or entering the Multiboot2 parser.
- `tag scan complete`, `usable memory map found`, and
  `modules checked...`: MBI parsing and module-range validation completed.
- `building identity map` / `mapping high kernel`: the bootstrap is
  preparing its initial 4-level page tables.
- `loading CR3` without `pre_init complete`: inspect the page-table switch
  and the exception trace in `/tmp/minix-efi64-qemu.log`.
- `pre_init complete` but no normal MINIX banner: inspect the transition to
  `kmain()`, the high kernel stack, and `prot_init()`.

The QEMU log records CPU exceptions and reset events. The final serial marker
plus the exception vector, faulting RIP, and CR2 (for a page fault) should
pinpoint the next fix. Do not infer a successful boot solely from the
Multiboot2 handoff line: that line is printed by the EFI loader before it
jumps to the kernel.
