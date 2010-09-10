#!/bin/sh
if [ $# -gt 0 ]
then	ARGS="-args $@"
fi
/bin/service up /sbin/readclock.drv -period 5HZ -script /etc/rs.single $ARGS
