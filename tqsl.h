#ifndef TQSL_H
#define TQSL_H
/*
  tqsl.c and tqsl.h is the interface to TrustedQSL API

  $Id$
*/

int tqslReadCert(char *fname,TqslCert *);
int tqslWriteCert(char *fname,TqslCert *);
int tqslCheckCert(TqslCert *cert,AmCertExtern *CACert);
int tqslSignCert(TqslCert *cert,const char *caPrivFName,
		 const char *caId,const char *pubKeyFName,
		 const char *certNum,const char *issueDate,
		 char *expireDate);
int tqslSignData(const char *privKeyFname,const unsigned char *data,
		 TqslCert *cert,TqslSignature *signature);
  
  



#endif
