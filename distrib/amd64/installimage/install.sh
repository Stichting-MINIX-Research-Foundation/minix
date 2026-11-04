#!/bin/sh
# Entry point used by the installation media's automatic console login.
PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH

/setup-amd64.sh
status=$?

echo
if [ "$status" -eq 0 ]; then
	echo "The installer has finished. You may reboot or use this shell."
else
	echo "The installer stopped with status $status. Review the message above."
fi
exec /bin/sh -i
