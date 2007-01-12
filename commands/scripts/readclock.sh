#!/bin/sh
if [ $# -gt 0 ]
then	ARGS="-args $@"
fi
/bin/service up /sbin/readclock.drv -config /etc/drivers.conf -script /etc/rs.single $ARGS
