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
const int signSizeMax=120;
const int pubKeySize=176;
const int CALL_SIZE=10; 

struct TqslPublicKey
{
  char			pkType;
  char			callSign[12];
  char			pubkeyNum[5];
  unsigned char		pkey[pubKeySize];
};

struct TqslCertData
{
  char			certType;
  char			issueDate[10];
  char			expireDate[10];
  char			caID[10];
  char			caPK[4];
  char			caCertNum[6];
  TqslPublicKey		publicKey;
};
struct TqslCert
{
  TqslCertData		data;
  char		        sigSize[3];
  char			signature[signSizeMax];
};

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

  int tqslReadCert(const char *fname,TqslCert *);
  int tqslWriteCert(const char *fname,TqslCert *);
  int tqslGenNewKeys(const char *callSign,const char *privFile,
		    const char *pubFile);
  int tqslCheckCert(TqslCert *cert,TqslCert *CACert);
  int tqslSignCert(TqslCert *cert,const char *caPrivFName,
		 const char *caId,const char *pubKeyFName,
		 const char *certNum,const char *issueDate,
		 char *expireDate);
  int tqslSignData(const char *privKeyFname,const unsigned char *data,
		 TqslCert *cert,TqslSignature *signature);
  int tqslVerifyData(TqslCert *cert,unsigned char *data,
		     TqslSignature *signature);
  
#ifdef __cplusplus
}
#endif
#endif
