# MINIX 3 — Modern Desktop Support

This fork of [MINIX 3](https://www.minix3.org) works to give MINIX the
low-level support a modern desktop needs, the support Linux provides
today. It keeps MINIX a microkernel throughout.

The first target is a working **Hyprland** Wayland compositor on MINIX
3.4.0 (i386), running in QEMU. Hyprland was chosen because it uses almost
everything a modern Linux desktop needs:
- kernel modesetting
- shared-memory buffers
- event-driven I/O
- input devices
- seat management
- OpenGL
- threads

If Hyprland runs, most other desktop software can run too.

> **Status:** early, active development. The OS groundwork, the graphics
> device and the Wayland libraries are done and tested. There is no
> desktop yet. See [Roadmap](#roadmap).

---

## Mission

Give MINIX 3 the system interfaces that modern desktop software expects,
so that unmodified or lightly patched open-source desktop software
(Wayland compositors, toolkits, terminals, GL) builds and runs on it.

Linux desktop software uses Linux interfaces beyond POSIX:
- `epoll`, `eventfd`, `timerfd`, `signalfd`
- writable shared memory mappings
- DRM/KMS for display
- evdev/libinput for input
- logind/libseat for seat management
- udev for device discovery
- dma-buf for buffer sharing
- real threads

This project builds each of these on MINIX. Where possible, a piece is a
library or user-space driver. A change to the core servers is made only
when the feature cannot work without it.

## Vision

A MINIX that runs a modern graphical desktop: a reliable, inspectable
microkernel system that people can actually use day to day, and a
platform for showing that a fault-isolating design and a modern user
experience are compatible.

In concrete terms:

1. **Same desktop interfaces as Linux.** A program written against the
   Linux desktop stack (Wayland, libdrm, libinput, Mesa, pthreads)
   compiles for MINIX with a cross toolchain and small, documented
   patches. The patches should get smaller over time.
2. **Microkernel properties kept.** Graphics, input and seat services are
   ordinary isolated MINIX processes that the reincarnation server can
   restart. A crashing display driver should not take the system down.
3. **Tested at every step.** Every feature ships with in-guest regression
   tests and a reproducible gate run in a headless QEMU harness. "It
   booted once" is not a result.
4. **Upstream-quality changes.** In-tree work follows MINIX and NetBSD
   conventions: set lists, `system.conf` privileges, `docs/UPDATING`
   entries and `#ifdef __minix` guards in NetBSD-derived code. That way it
   stays reviewable and could be sent upstream.

### Linux parity scorecard

This compares what the Linux desktop stack relies on with what MINIX has
today. Status: ✅ done, 🟡 partial or emulated, 🚧 planned in the
roadmap, ❌ missing or open problem.

| Capability | Linux | MINIX (this fork) |
|---|---|---|
| Modern C/C++ toolchain | gcc/clang, C++20+ | ✅ gcc 13.3 + binutils 2.43 cross toolchain (C++17 verified in the guest) |
| `eventfd` / `timerfd` / `signalfd` | kernel | 🟡 libc emulation (socketpair, multiplexed itimer, pipe); handles the "block the signal" pattern |
| `epoll` | kernel | 🟡 libc emulation over `poll()`; level-triggered only, no `EPOLLET`; epoll fds can be polled and nested, but not `select()`ed |
| POSIX shm + writable `MAP_SHARED` files | ✅ | 🟡 works, including sparse files; changes are not written back to disk (not needed for `wl_shm`) |
| `mmap` of device memory | ✅ | ✅ character-device `mmap` (`FIOMMAP`) with VM privilege checks |
| DRM/KMS display | DRM drivers | 🟡 "DRM-lite" `/dev/drm0` for QEMU's Bochs/stdvga card: dumb buffers, mode setting, page flips, vblank events; libdrm emulates the KMS API |
| Atomic KMS, PRIME/dma-buf | ✅ | ❌ not yet (PRIME is needed for wlroots; planned for Phase 8) |
| Wayland libraries | ✅ | ✅ libwayland, wayland-protocols, pixman, xkbcommon, libffi |
| Wayland compositor | ✅ | ✅ Weston 14 on `/dev/drm0` with the pixman renderer: desktop shell, panel, `weston-terminal` with working keyboard and mouse |
| Fonts and 2D drawing | ✅ | ✅ freetype, fontconfig, cairo, libpng, DejaVu fonts |
| Input (evdev/libinput) | ✅ | ✅ evdev devices `/dev/input/event0` (keyboards) and `event1` (mice) in the input server; libevdev and libinput ported. No mouse wheel yet |
| Seat/session (logind, libseat) | ✅ | 🟡 libseat with its no-op backend: opens devices directly, no VT switching |
| Device discovery (udev) | ✅ | 🟡 `libudev-minix`: the libudev API over a fixed view of the input and display devices; no hotplug |
| OpenGL / EGL / GBM | Mesa + GPU | 🚧 Phase 7: Mesa softpipe (software), mini-GBM over dumb buffers |
| Threads | 1:1 kernel threads | ❌ libmthread only: cooperative user-level threads behind `<pthread.h>` (now with recursive mutexes and zero-initialized locks), so one blocking system call stalls the whole process. This is the main open problem |
| GPU acceleration, real hardware display | ✅ | ❌ out of scope for now (QEMU first) |
| Audio, USB HID, power management | ✅ | ❌ not started |

---

## Roadmap

Each phase has a gate: a test that must pass inside MINIX under QEMU
before the phase counts as done. Details, notes and known problems are in
[`PLAN.md`](PLAN.md).

| Phase | Goal | Status |
|---|---|---|
| 0 | Headless QEMU test harness | ✅ done |
| 1 | Modern cross toolchain (`i586-elf32-minix` gcc 13) | ✅ done |
| 2 | libc and VM gaps: event fds, shm, writable shared `mmap` | ✅ done |
| 3 | Graphics device: DRM-lite driver, device `mmap`, libdrm port | ✅ done |
| 4 | Wayland library stack, epoll, pthread compatibility | ✅ done |
| 5 | Input: evdev devices, libinput, libseat, libudev replacement | ✅ done |
| 6 | **Weston** with software rendering: first usable desktop | ✅ done |
| 7 | OpenGL: Mesa softpipe, EGL, GBM | 🚧 next |
| 8 | **Hyprland**: PRIME, wlroots, Hyprland port | planned |
| 9 | Session integration: foot terminal, fonts, config, image packaging | planned |

After Hyprland, possible next steps include:
- kernel-supported threads
- display drivers for real hardware
- more input devices
- audio
- shrinking the patch set so stock upstream releases build unchanged

---

## Getting started

All host-side work runs inside a Docker container (`minix-dev`, Debian
12), so build results do not depend on the host distribution. Build
inside the container; the results also run on the host. Details:
[`docker/README.md`](docker/README.md).

```sh
# One-time: build the dev container image
docker/dev.sh image

# Build the OS and a bootable disk image (-> minix_x86.img)
docker/dev.sh env OBJ=../obj.docker.i386 JOBS=$(nproc) ./releasetools/x86_hdimage.sh

# Boot it headless and run a command inside MINIX
docker/dev.sh ./releasetools/x86_qemu.py --raw --cmd 'uname -a'

# Run this project's regression tests inside MINIX
docker/dev.sh ./releasetools/x86_qemu.py --raw \
    --cmd 'cd /usr/tests/minix-posix && ./run -t "95 96 97 98"'
```

### The QEMU harness

`releasetools/x86_qemu.py` (wrapper: `x86_qemu.sh`):
- boots `minix_x86.img` headless and logs in as root over a serial
  console;
- can attach an ext2 "payload" disk with test binaries (`--payload`);
- runs `--cmd` commands and checks for `--expect` text;
- can capture the screen (`--screendump`).

Exit status 0 means every command succeeded. All of this project's gates
use it.

### Ports (third-party desktop software)

The cross toolchain and the ported libraries live outside the OS build,
in a sibling directory `../minix-ports/`:
- `build-toolchain.sh`
- `build-port.sh <name>`
- per-port recipes and MINIX patches
- gate scripts such as `test-drm.sh` and `test-wayland.sh`

Ports install under `/usr/pkg` in the guest. See `minix-ports/README.md`.

---

## What changed in the OS so far

These are the main in-tree changes. The full list is in
[`PLAN.md`](PLAN.md), and upgrade notes are in
[`docs/UPDATING`](docs/UPDATING).

- **Build:** cross-builds with modern host compilers (GCC 10+); Docker dev
  environment; process start-up supports both `.init_array` and the
  legacy `_init`.
- **VM / VFS:**
  - writable `MAP_SHARED` file mappings, including sparse files;
  - `EACCES` for writable shared mappings of read-only fds;
  - `mmap()` of character devices through a driver `FIOMMAP` ioctl.
- **PM:** fork children start with no pending signals, as POSIX requires.
- **libc:**
  - `eventfd`, `timerfd`, `signalfd` (including blocked signals), `epoll`;
  - `shm_open`/`shm_unlink` (over `/dev/shm`), `msync`, `pthread_sigmask`;
  - `select()` restarts after signals with `SA_RESTART`, as POSIX
    requires;
  - `poll()` on an epoll fd reports it ready when a member is, so epoll
    fds can be polled and nested.
- **Threads:** `<pthread.h>` and `libpthread` on top of libmthread; gcc
  accepts `-pthread`; mutex types (recursive, error-checking) and
  zero-initialized mutexes and condition variables.
- **More libc:** `posix_fadvise`, `strchrnul`, C11 `static_assert`.
- **Drivers:** `bochs`, the DRM-lite display driver for QEMU/Bochs stdvga
  (`/dev/drm0`, major 18).
- **Input:** the input server offers Linux evdev devices
  (`/dev/input/event0` for keyboards, `event1` for mice); the PS/2 mouse
  driver reports Y movement in the HID direction.
- **Tests:** `test95` (shared mmap), `test96` (eventfd/timerfd/signalfd),
  `test97` (device mmap and DRM-lite), `test98` (epoll), `test99`
  (evdev devices, optionally with real keyboard and mouse input),
  `test100` (pthread mutex types and initialization).

## Repository layout

The tree has NetBSD's source layout. Most top-level directories (`bin`,
`lib`, `usr.bin`, `sys`, `external`, …) are NetBSD userland and build
infrastructure. MINIX-specific code is under `minix/`:

| Path | Contents |
|---|---|
| `minix/kernel/` | the microkernel |
| `minix/servers/` | system servers: `pm`, `vfs`, `vm`, `rs`, `ds`, … |
| `minix/drivers/` | device drivers, each its own process (including `video/bochs`) |
| `minix/fs/` | file system servers |
| `minix/lib/` | MINIX libraries (`libsys`, driver frameworks, libc parts, `libmthread`) |
| `minix/tests/` | MINIX regression suite (runs inside the guest) |
| `releasetools/` | image builders and the QEMU harness |
| `docker/` | container dev environment |
| `PLAN.md` | roadmap, status and known pitfalls |

## Contributing

- Every installed file must be listed in `distrib/sets/lists/`, or the
  release build fails its file-list check.
- New tests go in `minix/tests/Makefile` and the `alltests` list in
  `minix/tests/run`.
- A new or changed service needs its privileges declared in
  `system.conf`.
- Record changes that affect upgrades in `docs/UPDATING`.

## License and credits

MINIX 3 is © Vrije Universiteit Amsterdam and contributors, under a
BSD-style license (see [`LICENSE`](LICENSE)). The NetBSD-derived parts
keep their original licenses. Ported third-party software keeps its own
license.

This project builds on the work of the MINIX 3 team and the Stichting
MINIX Research Foundation.
