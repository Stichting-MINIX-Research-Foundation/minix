#!/bin/sh
#
# poweroff 1.0 - power off the system		Author: David van Moolenbroek
#								  12 Jun 2009

if [ $# -gt 0 ]; then
  echo "usage: poweroff" >&2
  exit 1
fi

PATH=/usr/bin:$PATH

exec shutdown -p
