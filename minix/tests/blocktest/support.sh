# Supporting routines for blocktest. Do not run directly.

# usage: devtopair /dev/cXdY..
# returns a label, minor pair in the form "label=at_wini_N,minor=M"
devtopair() {
  label=`awk "/^$(stat -f '%Hr' $1) / "'{print $2}' /proc/dmap`
  if [ ! -z "$label" ]; then echo "label=$label,minor=`stat -f '%Lr' $1`"; fi
}

# usage: block_test /dev/cXdY.. "params,for,blocktest"
# runs the blocktest driver on the given device with the given parameters
block_test() {
  if [ ! -x blocktest ]; then echo "compile blocktest first!" >&2; exit 1; fi
  if [ ! -b "$1" ]; then echo "$1 is not a block device" >&2; exit 1; fi
  pair=$(devtopair $1)
  if [ -z "$pair" ]; then echo "driver not found for $1" >&2; exit 1; fi
  minix-service up `pwd`/blocktest -args "$pair,$2" -config system.conf \
    -script /etc/rs.single -label blocktest_$(stat -f '%r' $1)
}
