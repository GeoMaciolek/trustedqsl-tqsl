/* 
    TrustedQSL/LoTW client and PKI Library
    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net and
                     ARRL

    This library is free software; you can redistribute it and/or
    modify it under the terms of the Open License packaged with
    the source.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

    You should have received a copy of the Open License see 
    http://www.trustedQSL.org/ or contact wa1gon@arrl.net.

*/


#ifndef SIGN_H
#define SIGN_H

#ifndef TQSL_H
#include "tqsl.h"
#endif
#include <openssl/bn.h>

#define ERROR -1
int 		sha1File(char *fname,unsigned char *);
int 		writeSign(char *fname,unsigned char *sig,int len);
qqqint 		readSign(char *fname,unsigned char *sig,int len);
int		readBig(const char *fname,BIGNUM **bn);
int 		readSignHex(char *fname,unsigned char *sig,int len);
TqslPublicKey 	*readPubKey(char *fname,char *typ);
int 		readCert(char *fname,TqslCert *cert);
int 		validateCert(TqslCert *caCert, TqslCert *amCert);
//void 		initPublicKey(TqslPublicKey *pk);
void		dumpCert(TqslCert *cert,FILE *fp);
void		dumpPubKey(TqslPublicKey *pk,FILE *fp);
int		chkDate(const char *);


const char pVal[] = "F14CF22587FDE3F8BC0AEEF105B8981A724B70E4EA82AA1CE0A"
    "ED02030E10FA6362E6418D41C7BDAF374A818006A7A690E95C44EF60B6B329602"
    "1E205D3D50A7EC8B7633B362399623D09047264EB86643AE47DA22F9DC61";

const char gVal[] = "1A337E37A6C7B32C0DDAE04CF93BADC79BD4D781C39E5E9D141"
     "14C1E8ACA0B8ABAB81F5685305597A23EB5F4CD55508729FCF32DF488FC05FE6BD"
     "AF5AAD3E1308BDFC2FE80E95FB5595B66CAC4C42A5A8A3F62449EE28392";

const char qVal[] = "CEAAC08334A9071F79DF95789C938A81BFB7F24D";

#endif SIGN_H

