#!/bin/sh

# Supporting routines for ddekit Do not run directly.

# usage: run_ddekittest
# runs the ddekit driver on the given device with the given parameters
run_ddekittest () {
  if [ ! -x ddekittest_driver ]; then echo "compile ddekittest first!" >&2; exit 1; fi
  minix-service up `pwd`/ddekittest_driver  -config system.conf \
	-script /etc/rs.single -label ddekittest
}


#
# We do not have much here just calling the source run_ddekittest here
#
run_ddekittest
sleep 10
minix-service down ddekittest
