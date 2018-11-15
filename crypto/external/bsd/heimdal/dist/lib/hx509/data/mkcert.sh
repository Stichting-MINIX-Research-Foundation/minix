#! /bin/bash

set -e

# For now, avoid going past the 2038 32-bit clock rollover
DAYS=$(( ( 0x7fffffff - $(date +%s) ) / 86400 - 1 ))

key() {
    local key=$1; shift

    if [ ! -f "${key}.pem" ]; then
	openssl genpkey \
	    -paramfile <(openssl ecparam -name prime256v1) \
	    -out "${key}.pem"
    fi
}

req() {
    local key=$1; shift
    local dn=$1; shift

    openssl req -new -sha256 -key "${key}.pem" \
	-config <(printf "[req]\n%s\n%s\n[dn]\nCN_default=foo\n" \
		   "prompt = yes" "distinguished_name = dn") \
	-subj "${dn}"
}

cert() {
    local cert=$1; shift
    local exts=$1; shift

    openssl x509 -req -sha256 -out "${cert}.pem" \
	-extfile <(printf "%s\n" "$exts") "$@"
}

genroot() {
    local dn=$1; shift
    local key=$1; shift
    local cert=$1; shift

    exts=$(printf "%s\n%s\n%s\n%s\n" \
	   "subjectKeyIdentifier = hash" \
	   "authorityKeyIdentifier  = keyid" \
	   "basicConstraints = CA:true" \
	   "keyUsage = keyCertSign, cRLSign" )
    key "$key"; req "$key" "$dn" |
	cert "$cert" "$exts" -signkey "${key}.pem" \
	    -set_serial 1 -days "${DAYS}"
}

genee() {
    local dn=$1; shift
    local key=$1; shift
    local cert=$1; shift
    local cakey=$1; shift
    local cacert=$1; shift

    exts=$(printf "%s\n%s\n%s\n%s\n" \
	    "subjectKeyIdentifier = hash" \
	    "authorityKeyIdentifier = keyid, issuer" \
	    "basicConstraints = CA:false" \
	    "keyUsage = digitalSignature, keyEncipherment, dataEncipherment" \
	)
    key "$key"; req "$key" "$dn" |
	cert "$cert" "$exts" -CA "${cacert}.pem" -CAkey "${cakey}.pem" \
	    -set_serial 2 -days "${DAYS}" "$@"
}


genroot "/C=SE/O=Heimdal/CN=CA secp256r1" \
	secp256r1TestCA.key secp256r1TestCA.cert
genee "/C=SE/O=Heimdal/CN=Server" \
	secp256r2TestServer.key secp256r2TestServer.cert \
	secp256r1TestCA.key secp256r1TestCA.cert
genee "/C=SE/O=Heimdal/CN=Client" \
	secp256r2TestClient.key secp256r2TestClient.cert \
	secp256r1TestCA.key secp256r1TestCA.cert

cat secp256r1TestCA.key.pem secp256r1TestCA.cert.pem > \
	secp256r1TestCA.pem
cat secp256r2TestClient.cert.pem secp256r2TestClient.key.pem > \
	secp256r2TestClient.pem
cat secp256r2TestServer.cert.pem secp256r2TestServer.key.pem > \
	secp256r2TestServer.pem
