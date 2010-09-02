#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin
sed -n '1,/@ACPI/p' <proto | grep -v @ACPI@
if [ -e acpi ]
then
echo "		acpi ---755 0 0 acpi"
fi
sed -n '/@ACPI/,/@DEV/p' <proto  | grep -v -e @ACPI@ -e @DEV@
(
cd /dev
ls -aln | grep '^[bc]' | egrep -v ' (fd1|fd0p|tcp|eth|ip|udp|tty[pq]|pty)' | grep -v 13, | \
sed	-e 's/^[bc]/& /' -e 's/rw-/6/g' -e 's/r--/4/g' \
	-e 's/-w-/2/g'  -e 's/---/0/g' | \
awk '{ printf "\t\t%s %s--%s %d %d %d %d \n", $11, $1, $2, $4, $5, $6, $7; }'
)
sed -n '/@DEV/,$p' <proto  | grep -v @DEV@
