@echo off
set OPENSSL_CONF=test-sign.cnf

set qnotbefore=%1
set qnotafter=%2
set dxcc=%3

rem ######## Sign the CRQ to produce the cert
rem ########
echo ==== Signing Cert
openssl x509 -req -in reqtmp.x509 -extensions v3_ARRL -days 365 -extfile %OPENSSL_CONF% -CA TestCAproductioncert.pem -CAkey TestCAproductionkeys.pem -CAcreateserial -out certtmp
