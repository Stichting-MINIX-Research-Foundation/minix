#!/bin/sh

echo "Applying blacklist..."
cd netbsd
while read bl
do
	echo $bl
	rm -r $bl
done
cd ..
