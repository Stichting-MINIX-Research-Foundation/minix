
{
	find . -name "dnskey.db"
	find . -name "dsset-*"
	find . -name "keyset-*"
	find . -name "K*"
} | xargs rm 


for file in `find . -name "zone.db.signed"`
do
	cp /dev/null $file
done
