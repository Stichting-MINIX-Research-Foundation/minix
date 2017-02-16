

ZKT_CONFFILE=dnssec.conf
export ZKT_CONFFILE

if true
then
	echo "All internal keys:"
	./dnssec-zkt-intern
	echo

	echo "All external keys:"
	./dnssec-zkt-extern
	echo
fi

echo "Sign both views"
./dnssec-signer-intern -v -v -f -r
echo
./dnssec-signer-extern -v -v
