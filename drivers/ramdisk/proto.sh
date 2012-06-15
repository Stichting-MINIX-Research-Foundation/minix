#!/bin/sh

set -e

PATH=/bin:/sbin:/usr/bin:/usr/sbin
PROTO=${1:-proto}
sed -n '1,/@ACPI/p' <${PROTO} | grep -v @ACPI@
if [ -e acpi ]
then
echo "		acpi ---755 0 0 acpi"
fi
sed -n '/@ACPI/,/@DEV/p' <${PROTO}  | grep -v -e @ACPI@ -e @DEV@
cat proto.dev
sed -n '/@DEV/,$p' <${PROTO}  | grep -v @DEV@
cat proto.common.etc
if [ -x /libexec/ld.elf_so ]
then	cat proto.common.dynamic
fi
echo '$'
