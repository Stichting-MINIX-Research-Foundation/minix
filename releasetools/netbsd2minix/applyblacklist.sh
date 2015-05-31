#!/bin/sh

echo "Applying blacklist..."
cd src
while read bl
do
	echo $bl
	rm -r $bl
done
cd ..
