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
const int signSizeMax=500;
const int pubKeySize=200;
const int CALL_SIZE=10;
const int MaxCertSize=500; 

struct TqslPublicKey
{                                        
  char			pkType;          
  char			callSign[13];    
  char			pubkeyNum[6];    
  unsigned char		pkey[pubKeySize];
};                                       

struct TqslCertData
{                                        
  char			certType;        
  char			issueDate[11];   
  char			expireDate[11];  
  char			caID[11];        
  char			caCall1[13];
  char			caCall2[13];
  char			caCall3[13];
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
  char			yourCall[CALL_SIZE];
  char			theirCall[CALL_SIZE];
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

  char * 	tqslSigToStr(TqslSignature *);
  int 		tqslStrToSig(TqslSignature *,char *);

  char * 	tqslPubKeyToStr(TqslPublicKey *);
  int 		tqslStrToPubKey(TqslPublicKey *,const char *);

  char * 	tqslCertToStr(TqslCert *);
  int 		tqslStrToCert(TqslCert *,const char *);

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
