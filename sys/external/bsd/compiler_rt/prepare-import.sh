#!/bin/sh
# $NetBSD: prepare-import.sh,v 1.4 2014/05/16 00:08:17 joerg Exp $
#
# Checkout compiler_rt into dist.
# Run this script and check for additional files and directories to prune,
# only relevant content should be included.

set -e

cd dist
rm -rf .svn
rm -rf SDKs android cmake include make third_party unittests www
rm -f .arcconfig .gitignore CMakeLists.txt Makefile
rm -rf lib/BlocksRuntime lib/asan lib/dfsan lib/interception lib/lsan
rm -rf lib/msan lib/msandr lib/sanitizer_common lib/tsan lib/ubsan
rm -rf test/BlocksRuntime test/asan test/dfsan test/lit.* test/lsan test/msan test/sanitizer_common test/tsan test/ubsan
rm -f lib/*/*/Makefile.mk lib/*/Makefile.mk */Makefile.mk
rm -f lib/*/CMakeLists.txt */CMakeLists.txt
rm -f lib/builtins/apple_versioning.c lib/lit.common.*
