#!/bin/sh

set -e
echo "Updating package database.."
pkgin update
echo "Making available package list.."
packages="`pkgin av | awk '{ print $1 }'`"
echo "Made list of `echo $packages | wc -w` packages."
echo $packages | xargs -n50 pkgin -y in
