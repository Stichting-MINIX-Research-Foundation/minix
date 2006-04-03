#!/bin/sh
rm -rf t
mkdir t 2>/dev/null || true
cd t
MAKEDEV std 
rm fd1* fd0p* tcp* eth* ip* udp* tty[pq]* pty*
sed -n '1,/@DEV/p' <../proto  | grep -v @DEV@
ls -aln | grep '^[bc]' | \
sed	-e 's/^[bc]/& /' -e 's/rw-/6/g' -e 's/r--/4/g' \
	-e 's/-w-/2/g'  -e 's/---/0/g' | \
awk '{ printf "\t\t%s %s--%s %d %d %d %d \n", $11, $1, $2, $4, $5, $6, $7; }'
sed -n '/@DEV/,$p' <../proto  | grep -v @DEV@
