#!/bin/sh
./setupnetbsd.sh
./applyblacklist.sh < blacklist.txt
./setupminix.sh
cd src
make build
