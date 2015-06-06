cd $N2M/whitelist
for item in `ls`
do
	$N2M/applywhitelist.sh $NETBSD/$item $item < $item
	echo "$item moved"
done

$N2M/applywhitelist.sh $MINIX . < $N2M/minix.txt

cd $MINIX
