#!/bin/sh
#
# usage test-sign qsonotbefore qsonotafter dxcc

export OPENSSL_CONF=test-sign.cnf

qnotbefore=$1
qnotafter=$2
dxcc=$3

export qnotbefore qnotafter dxcc

######## Sign the CRQ to produce the cert
########
echo "==== Signing Cert"
openssl x509 -req -in reqtmp.x509 -extensions v3_ARRL -days 365 \
  -extfile $OPENSSL_CONF -CA TestCAproductioncert.pem -CAkey \
  TestCAproductionkeys.pem -CAcreateserial -out certtmp
