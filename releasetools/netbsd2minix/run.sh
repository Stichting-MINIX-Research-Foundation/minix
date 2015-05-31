#!/bin/sh
./applyblacklist.sh < blacklist.txt
./setupnetbsd.sh
./setupminix.sh
cd src
make build
