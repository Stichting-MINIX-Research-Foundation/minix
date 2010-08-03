#!/bin/sh

# Actually, newroot is just like mounting on the root
exec mount -n "$*" / > /dev/null

