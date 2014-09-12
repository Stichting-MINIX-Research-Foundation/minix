#!/bin/sh

CORE_TOOLS="openssh vim curl"
DEV_TOOLS="git-base bmake gmake binutils clang"
EXTRA_TOOLS="bison groff perl python27"

cat <<START_EOT
This script contains 3 sets of packages, you can install any of those
sets independently, just type N when pkgin ask for confirmation to skip
a set.

The following sets are available:

1. Core tools:
${CORE_TOOLS}

2. Dev tools:
${DEV_TOOLS}

3. Extras:
${EXTRA_TOOLS}

START_EOT

echo "Installing core tools"
echo "====================="
echo

pkgin install ${CORE_TOOLS}

echo
echo "Installing default development tools"
echo "===================================="
echo

pkgin install ${DEV_TOOLS}

echo
echo "Installing extras"
echo "================="
echo

pkgin install ${EXTRA_TOOLS}
