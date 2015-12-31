// General tests that the header search paths detected by the driver and passed
// to CC1 are sane.
//
// Test a simulated installation of libc++ on Linux, both through sysroot and
// the installation path of Clang.
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu \
// RUN:     -stdlib=libc++ \
// RUN:     -ccc-install-dir %S/Inputs/basic_linux_tree/usr/bin \
// RUN:     --sysroot=%S/Inputs/basic_linux_libcxx_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-BASIC-LIBCXX-SYSROOT %s
// CHECK-BASIC-LIBCXX-SYSROOT: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-BASIC-LIBCXX-SYSROOT: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-BASIC-LIBCXX-SYSROOT: "-internal-isystem" "[[SYSROOT]]/usr/include/c++/v1"
// CHECK-BASIC-LIBCXX-SYSROOT: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu \
// RUN:     -stdlib=libc++ \
// RUN:     -ccc-install-dir %S/Inputs/basic_linux_libcxx_tree/usr/bin \
// RUN:     --sysroot=%S/Inputs/basic_linux_libcxx_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-BASIC-LIBCXX-INSTALL %s
// CHECK-BASIC-LIBCXX-INSTALL: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-BASIC-LIBCXX-INSTALL: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-BASIC-LIBCXX-INSTALL: "-internal-isystem" "[[SYSROOT]]/usr/bin/../include/c++/v1"
// CHECK-BASIC-LIBCXX-INSTALL: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
//
// Test a very broken version of multiarch that shipped in Ubuntu 11.04.
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target i386-unknown-linux \
// RUN:     --sysroot=%S/Inputs/ubuntu_11.04_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-11-04 %s
// CHECK-UBUNTU-11-04: "{{.*}}clang{{.*}}" "-cc1"
// CHECK-UBUNTU-11-04: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-11-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/i386-linux-gnu/gcc/i686-linux-gnu/4.5/../../../../../include/c++/4.5"
// CHECK-UBUNTU-11-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/i386-linux-gnu/gcc/i686-linux-gnu/4.5/../../../../../include/c++/4.5/i686-linux-gnu"
// CHECK-UBUNTU-11-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/i386-linux-gnu/gcc/i686-linux-gnu/4.5/../../../../../include/c++/4.5/backward"
// CHECK-UBUNTU-11-04: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-UBUNTU-11-04: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-UBUNTU-11-04: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-UBUNTU-11-04: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
//
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu \
// RUN:     --sysroot=%S/Inputs/ubuntu_13.04_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-13-04 %s
// CHECK-UBUNTU-13-04: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-UBUNTU-13-04: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-13-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7"
// CHECK-UBUNTU-13-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/x86_64-linux-gnu/c++/4.7"
// CHECK-UBUNTU-13-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7/backward"
// CHECK-UBUNTU-13-04: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-UBUNTU-13-04: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-UBUNTU-13-04: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/x86_64-linux-gnu"
// CHECK-UBUNTU-13-04: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-UBUNTU-13-04: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
//
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnux32 \
// RUN:     --sysroot=%S/Inputs/ubuntu_14.04_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-14-04 %s
// CHECK-UBUNTU-14-04: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-UBUNTU-14-04: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-14-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8"
// CHECK-UBUNTU-14-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/x86_64-linux-gnu/c++/4.8/x32"
// CHECK-UBUNTU-14-04: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8/backward"
// CHECK-UBUNTU-14-04: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-UBUNTU-14-04: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|x32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-UBUNTU-14-04: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/x86_64-linux-gnu"
// CHECK-UBUNTU-14-04: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-UBUNTU-14-04: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
///
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target arm-linux-gnueabihf \
// RUN:     --sysroot=%S/Inputs/ubuntu_13.04_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-13-04-CROSS %s
// CHECK-UBUNTU-13-04-CROSS: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-UBUNTU-13-04-CROSS: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-13-04-CROSS: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc-cross/arm-linux-gnueabihf/4.7/../../../../include/c++/4.7"
// CHECK-UBUNTU-13-04-CROSS: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc-cross/arm-linux-gnueabihf/4.7/../../../../include/arm-linux-gnueabihf/c++/4.7"
// CHECK-UBUNTU-13-04-CROSS: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc-cross/arm-linux-gnueabihf/4.7/../../../../include/c++/4.7/backward"
// CHECK-UBUNTU-13-04-CROSS: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-UBUNTU-13-04-CROSS: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-UBUNTU-13-04-CROSS: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-UBUNTU-13-04-CROSS: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
//
// Test Ubuntu/Debian's new version of multiarch, with -m32.
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu -m32 \
// RUN:     --sysroot=%S/Inputs/ubuntu_13.04_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-13-04-M32 %s
// CHECK-UBUNTU-13-04-M32: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-UBUNTU-13-04-M32: "-triple" "i386-unknown-linux-gnu"
// CHECK-UBUNTU-13-04-M32: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-13-04-M32: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7"
// CHECK-UBUNTU-13-04-M32: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/x86_64-linux-gnu/c++/4.7/32"
// CHECK-UBUNTU-13-04-M32: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7/backward"
//
// Test Ubuntu/Debian's Ubuntu 14.04 config variant, with -m32
// and an empty 4.9 directory.
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu -m32 \
// RUN:     --sysroot=%S/Inputs/ubuntu_14.04_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-14-04-M32 %s
// CHECK-UBUNTU-14-04-M32: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-UBUNTU-14-04-M32: "-triple" "i386-unknown-linux-gnu"
// CHECK-UBUNTU-14-04-M32: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-14-04-M32: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8"
// CHECK-UBUNTU-14-04-M32: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/x86_64-linux-gnu/c++/4.8/32"
// CHECK-UBUNTU-14-04-M32: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.8/../../../../include/c++/4.8/backward"
//
// Test Ubuntu/Debian's Ubuntu 14.04 with -m32 and an i686 cross compiler
// installed rather than relying on multilib. Also happens to look like an
// actual i686 Ubuntu system.
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu -m32 \
// RUN:     --sysroot=%S/Inputs/ubuntu_14.04_multiarch_tree2 \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-14-04-I686 %s
// CHECK-UBUNTU-14-04-I686: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-UBUNTU-14-04-I686: "-triple" "i386-unknown-linux-gnu"
// CHECK-UBUNTU-14-04-I686: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-14-04-I686: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/i686-linux-gnu/4.8/../../../../include/c++/4.8"
// CHECK-UBUNTU-14-04-I686: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/i686-linux-gnu/4.8/../../../../include/i386-linux-gnu/c++/4.8"
// CHECK-UBUNTU-14-04-I686: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/i686-linux-gnu/4.8/../../../../include/c++/4.8/backward"
//
// Test Ubuntu/Debian's Ubuntu 14.04 for powerpc64le
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target powerpc64le-unknown-linux-gnu -m32 \
// RUN:     --sysroot=%S/Inputs/ubuntu_14.04_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-UBUNTU-14-04-PPC64LE %s
// CHECK-UBUNTU-14-04-PPC64LE: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-UBUNTU-14-04-PPC64LE: "-triple" "powerpc64le-unknown-linux-gnu"
// CHECK-UBUNTU-14-04-PPC64LE: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-UBUNTU-14-04-PPC64LE: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc64le-linux-gnu/4.8/../../../../include/c++/4.8"
// CHECK-UBUNTU-14-04-PPC64LE: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc64le-linux-gnu/4.8/../../../../include/powerpc64le-linux-gnu/c++/4.8"
// CHECK-UBUNTU-14-04-PPC64LE: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc64le-linux-gnu/4.8/../../../../include/c++/4.8/backward"
// CHECK-UBUNTU-14-04-PPC64LE: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/powerpc64le-linux-gnu"
// CHECK-UBUNTU-14-04-PPC64LE: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-UBUNTU-14-04-PPC64LE: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
//
// Thoroughly exercise the Debian multiarch environment.
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target i686-linux-gnu \
// RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-X86 %s
// CHECK-DEBIAN-X86: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-DEBIAN-X86: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-DEBIAN-X86: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/i686-linux-gnu/4.5/../../../../include/c++/4.5"
// CHECK-DEBIAN-X86: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/i686-linux-gnu/4.5/../../../../include/c++/4.5/i686-linux-gnu"
// CHECK-DEBIAN-X86: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/i686-linux-gnu/4.5/../../../../include/c++/4.5/backward"
// CHECK-DEBIAN-X86: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-DEBIAN-X86: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-DEBIAN-X86: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/i386-linux-gnu"
// CHECK-DEBIAN-X86: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-DEBIAN-X86: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-linux-gnu \
// RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-X86-64 %s
// CHECK-DEBIAN-X86-64: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-DEBIAN-X86-64: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-DEBIAN-X86-64: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.5/../../../../include/c++/4.5"
// CHECK-DEBIAN-X86-64: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.5/../../../../include/c++/4.5/x86_64-linux-gnu"
// CHECK-DEBIAN-X86-64: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-linux-gnu/4.5/../../../../include/c++/4.5/backward"
// CHECK-DEBIAN-X86-64: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-DEBIAN-X86-64: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-DEBIAN-X86-64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/x86_64-linux-gnu"
// CHECK-DEBIAN-X86-64: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-DEBIAN-X86-64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target powerpc-linux-gnu \
// RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-PPC %s
// CHECK-DEBIAN-PPC: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-DEBIAN-PPC: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-DEBIAN-PPC: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc-linux-gnu/4.5/../../../../include/c++/4.5"
// CHECK-DEBIAN-PPC: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc-linux-gnu/4.5/../../../../include/c++/4.5/powerpc-linux-gnu"
// CHECK-DEBIAN-PPC: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc-linux-gnu/4.5/../../../../include/c++/4.5/backward"
// CHECK-DEBIAN-PPC: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-DEBIAN-PPC: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-DEBIAN-PPC: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/powerpc-linux-gnu"
// CHECK-DEBIAN-PPC: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-DEBIAN-PPC: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target powerpc64-linux-gnu \
// RUN:     --sysroot=%S/Inputs/debian_multiarch_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-DEBIAN-PPC64 %s
// CHECK-DEBIAN-PPC64: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-DEBIAN-PPC64: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-DEBIAN-PPC64: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc64-linux-gnu/4.5/../../../../include/c++/4.5"
// CHECK-DEBIAN-PPC64: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc64-linux-gnu/4.5/../../../../include/c++/4.5/powerpc64-linux-gnu"
// CHECK-DEBIAN-PPC64: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/powerpc64-linux-gnu/4.5/../../../../include/c++/4.5/backward"
// CHECK-DEBIAN-PPC64: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-DEBIAN-PPC64: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-DEBIAN-PPC64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/powerpc64-linux-gnu"
// CHECK-DEBIAN-PPC64: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-DEBIAN-PPC64: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
//
// Test Gentoo's weirdness both before and after they changed it in their GCC
// 4.6.4 release.
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu \
// RUN:     --sysroot=%S/Inputs/gentoo_linux_gcc_4.6.2_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-GENTOO-4-6-2 %s
// CHECK-GENTOO-4-6-2: "{{.*}}clang{{.*}}" "-cc1"
// CHECK-GENTOO-4-6-2: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-GENTOO-4-6-2: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-pc-linux-gnu/4.6.2/include/g++-v4"
// CHECK-GENTOO-4-6-2: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-pc-linux-gnu/4.6.2/include/g++-v4/x86_64-pc-linux-gnu"
// CHECK-GENTOO-4-6-2: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-pc-linux-gnu/4.6.2/include/g++-v4/backward"
// CHECK-GENTOO-4-6-2: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-GENTOO-4-6-2: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-GENTOO-4-6-2: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-GENTOO-4-6-2: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target x86_64-unknown-linux-gnu \
// RUN:     --sysroot=%S/Inputs/gentoo_linux_gcc_4.6.4_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-GENTOO-4-6-4 %s
// CHECK-GENTOO-4-6-4: "{{.*}}clang{{.*}}" "-cc1"
// CHECK-GENTOO-4-6-4: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-GENTOO-4-6-4: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-pc-linux-gnu/4.6.4/include/g++-v4.6"
// CHECK-GENTOO-4-6-4: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-pc-linux-gnu/4.6.4/include/g++-v4.6/x86_64-pc-linux-gnu"
// CHECK-GENTOO-4-6-4: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/x86_64-pc-linux-gnu/4.6.4/include/g++-v4.6/backward"
// CHECK-GENTOO-4-6-4: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-GENTOO-4-6-4: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-GENTOO-4-6-4: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-GENTOO-4-6-4: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
//
// Check header search on Debian 6 / MIPS64
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target mips64-unknown-linux-gnuabi64 \
// RUN:     --sysroot=%S/Inputs/debian_6_mips64_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-MIPS64-GNUABI %s
// CHECK-MIPS64-GNUABI: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-MIPS64-GNUABI: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-MIPS64-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/mips64-linux-gnuabi64/4.9/../../../../include/c++/4.9"
// CHECK-MIPS64-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/mips64-linux-gnuabi64/4.9/../../../../include/c++/4.9/mips64-linux-gnuabi64"
// CHECK-MIPS64-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/mips64-linux-gnuabi64/4.9/../../../../include/c++/4.9/backward"
// CHECK-MIPS64-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-MIPS64-GNUABI: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-MIPS64-GNUABI: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/mips64-linux-gnuabi64"
// CHECK-MIPS64-GNUABI: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-MIPS64-GNUABI: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
//
// Check header search on Debian 6 / MIPS64
// RUN: %clang -no-canonical-prefixes %s -### -fsyntax-only 2>&1 \
// RUN:     -target mips64el-unknown-linux-gnuabi64 \
// RUN:     --sysroot=%S/Inputs/debian_6_mips64_tree \
// RUN:     --gcc-toolchain="" \
// RUN:   | FileCheck --check-prefix=CHECK-MIPS64EL-GNUABI %s
// CHECK-MIPS64EL-GNUABI: "{{[^"]*}}clang{{[^"]*}}" "-cc1"
// CHECK-MIPS64EL-GNUABI: "-isysroot" "[[SYSROOT:[^"]+]]"
// CHECK-MIPS64EL-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/mips64el-linux-gnuabi64/4.9/../../../../include/c++/4.9"
// CHECK-MIPS64EL-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/mips64el-linux-gnuabi64/4.9/../../../../include/c++/4.9/mips64el-linux-gnuabi64"
// CHECK-MIPS64EL-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/lib/gcc/mips64el-linux-gnuabi64/4.9/../../../../include/c++/4.9/backward"
// CHECK-MIPS64EL-GNUABI: "-internal-isystem" "[[SYSROOT]]/usr/local/include"
// CHECK-MIPS64EL-GNUABI: "-internal-isystem" "{{.*}}{{/|\\\\}}lib{{(64|32)?}}{{/|\\\\}}clang{{/|\\\\}}{{[0-9]\.[0-9]\.[0-9]}}{{/|\\\\}}include"
// CHECK-MIPS64EL-GNUABI: "-internal-externc-isystem" "[[SYSROOT]]/usr/include/mips64el-linux-gnuabi64"
// CHECK-MIPS64EL-GNUABI: "-internal-externc-isystem" "[[SYSROOT]]/include"
// CHECK-MIPS64EL-GNUABI: "-internal-externc-isystem" "[[SYSROOT]]/usr/include"
