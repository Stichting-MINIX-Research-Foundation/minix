#!/bin/sh
# Automatic console entry point for the amd64 installation media.
# Prefer the guided UEFI installer shipped in the ISO. The install-image
# target predates the UEFI media layout, so retain its legacy fallback.
PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH

if [ -x /usr/sbin/minix-install ] && [ -s /install-efiboot.img ]; then
	/usr/sbin/minix-install "$@"
	status=$?
else
	/setup-amd64.sh "$@"
	status=$?
fi

echo
if [ "$status" -eq 0 ]; then
	echo "The installer has finished. You may reboot or use this shell."
else
	echo "The installer stopped with status $status. Review the message above."
fi
exec /bin/sh -i
