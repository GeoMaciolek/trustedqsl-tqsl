#ifndef TQSL_H
#define TQSL_H
/*
  tqsl.c and tqsl.h is the interface to TrustedQSL API

  $Id$
*/

/*
    TrustedQSL Digital Signature Library
    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef  LITTLE_ENDIAN 
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#include <stdio.h>

#define TQSL_CA_CERT_HEX '0'         // self signed CA Cert (hex encoded)
#define TQSL_USER_CERT_HEX '1'       // CA  Signed User Cert (hex encode)
#define TQSL_CA_TEST_CERT_HEX '2'    // Test CA Cert.  Limited validation

// not done yet

#define TQSL_CA_CERT_B64 '3'         // self signed CA Cert (base 64 encoded)
#define TQSL_USER_CERT_B64 '4'       // CA  Signed User Cert (base 64 encode)
#define TQSL_CA_TEST_CERT_B64 '5'    // Test CA Cert.  Limited validation


#define TQSL_USER_SIGN_HEX '1'       // CA  Signed User Cert (hex encode)
#define TQSL_PUBLIC_KEY_HEX '1'      // standard hex encoded public

const int signSizeMax=500;
const int pubKeySize=200;
const int tqslCALL_SIZE=13;
const int MaxCertSize=500;

extern FILE   *dbFile;

struct TqslPublicKey
{                                        
  char			pkType;          
  char			callSign[tqslCALL_SIZE];
  char			pubkeyNum[6];    
  unsigned char		pkey[pubKeySize];
};                                       

struct TqslCertData
{                                        
  char			certType;        
  char			issueDate[11];   
  char			expireDate[11];  
  char			caID[tqslCALL_SIZE];
  char			caCall1[tqslCALL_SIZE];
  char			caCall2[tqslCALL_SIZE];
  char			caCall3[tqslCALL_SIZE];
  char			caCertNum[7];    
  TqslPublicKey		publicKey;       
};                                       

struct TqslCert
{      						//position  size 
  TqslCertData		data;                   //0          73
  // char		        sigSize[3];             //73          3
  char			signature[signSizeMax]; //76        120
};                                              // total size 196

struct TqslSignature
{
  char			sigType;
  char			signature[signSizeMax];
  TqslCert		cert;
};

struct TqslSignedQSL
{
  char			qslType;
  char			yourCall[tqslCALL_SIZE];
  char			theirCall[tqslCALL_SIZE];
  char			qDate[8];
  char			qTime[6];
  char			myRst[3];
  char			theirRst[3];
  char			county[25];
  char			city[25];
  char			state[2];
  char			grid[6];
  char			cqZone[2];
  char			ituZone[2];
  char			freq[10];
  char			band[7];
  char			mode[10];
  char			itoa[10];
  TqslSignature		signature;
  TqslCert		cert;
};

#ifdef __cplusplus
extern "C"
{
#endif
  int tqslEncryptPriv(char *privKeyStr,char *clrPass,char *privHashStr);
  int tqslDecryptPriv(char *privKeyStr,char *clrPass,char *oldPrivHashStr);
  void tqslHex2Bin(char *hexStr,unsigned char *binStr,int len);
  char *tqslBin2Hex(const unsigned char *binStr,int len);
  
  char * 	tqslSigToStr(TqslSignature *);
  int 		tqslStrToSig(TqslSignature *,char *);

  char * 	tqslPubKeyToStr(TqslPublicKey *);
  int 		tqslStrToPubKey(TqslPublicKey *,const char *);

  char * 	tqslCertToStr(TqslCert *);
  int 		tqslStrToCert(TqslCert *,const char *);
  int    tqslSha1(const unsigned char *,int,unsigned char *);
  int 		tqslReadCert(const char *fname,TqslCert *);
  int 		tqslWriteCert(const char *fname,TqslCert *);
  int 		tqslReadPub(const char *fname,TqslPublicKey *);
  int 		tqslWritePub(const char *fname,TqslPublicKey *);
  int 		tqslGenNewKeys(const char *callSign,char **,
		     TqslPublicKey *);
  int 		tqslCheckCert(TqslCert *cert,TqslCert *CACert,int chkCA);
  int 		tqslSignCert(TqslCert *cert,const char *caPrivKey,
			     const char *caId,TqslPublicKey *pubKey,
			     const char *certNum,const char *issueDate,
			     char *expireDate,int selfSign,char *call1,
			     char *call2,char *call3);
  int 		tqslSignData(const char *privKey,const unsigned char *data,
			     TqslCert *cert,TqslSignature *signature,int len);
  int 		tqslVerifyData(unsigned char *data,TqslSignature *signature,
			       int len);
  
#ifdef __cplusplus
}
#endif
#endif
