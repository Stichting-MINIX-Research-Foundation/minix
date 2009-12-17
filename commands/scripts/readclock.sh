#!/bin/sh
if [ $# -gt 0 ]
then	ARGS="-args $@"
fi
/bin/service up /sbin/readclock.drv -config /etc/system.conf -script /etc/rs.single $ARGS
