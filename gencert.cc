
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
#include <openssl/dsa.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <openssl/sha.h>

#include "sign.h"
#include "amcert.h"

extern int errno;
int debugLevel = 0;
static char cvsID[] = "$Id$";

int main(int argc, char *argv[])
{

  DSA    	*dsa;

  char  	*pk;
  char		typ;
  PublicKey   	*pubkey;
  AmCertExtern	amCert;

  int		rc;
  FILE		*fp;
  char		tmpStr[50];
  char		amPkFile[100];
  char		caPrivFile[100];
  char		amCertFile[100];
  char		certNum[10];
  int 		optCnt=0;
  int 		c,errFlg=0;
  int		selfSign=0;

  while ((c = getopt(argc, argv, "sC:p:a:n:d:")) != EOF)
    switch (c) 
      {
      case 's':
	selfSign = 1;
	break;
      case 'a':
	strcpy(amCertFile,optarg);
	optCnt++;
	break;
      case 'C':
	strcpy(caPrivFile,optarg);
	optCnt++;
	break;
      case 'd':
	debugLevel = atol(optarg);
	break;
      case 'p':
	strcpy(amPkFile,optarg);
	optCnt++;
	break;
      case 'n':
	strcpy(certNum,optarg);
	optCnt++;
	break;
      default:
	printf("invalid option %c\n",c);
	errFlg++;
	break;
      }

  if (errFlg || optCnt!= 4)
    {
      printf("usage: gencert -a Amateur-cert -p amateur-PK -C "
	     "CA-priv-key -n cert# \n");
      return(-1);
    }

  pk = (char *)readQpub(amPkFile,&typ);
  if (pk != NULL)
    {
      if (pk[0] == '1')
	{
	  pubkey = (PublicKey *)pk;
	}
    }

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(1);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  readBig(caPrivFile,&dsa->priv_key);

  memset(&amCert,0,sizeof(amCert));
  memset(&amCert,' ',sizeof(amCert)-1);
  
  if (selfSign == 1)
    amCert.data.certType = '0';
  else
    amCert.data.certType = '1';

  amCert.data.publicKey = *pubkey;
  memcpy(&amCert.data.issueDate,"03/04/2001",10);
  memcpy(&amCert.data.expireDate,"03/04/2011",10);

  
  sprintf(tmpStr,"%-10.10s       ","TQSL");
  memcpy(&amCert.data.caID,tmpStr,10);

  sprintf(tmpStr,"%s      ",certNum);
  memcpy(&amCert.data.caCertNum,tmpStr,6);

  unsigned char hash[40];
  SHA1((unsigned char *)&amCert.data,sizeof(amCert.data),hash);

  unsigned char sigRet[500];
  unsigned int sigLen;

  if (debugLevel > 0)
    {
      char *p;
      p = bin2hex(hash,SHA_DIGEST_LENGTH);
      printf("hash: %s\n",p); 
      printf("amcert: %s\n", (char *)&amCert);
    }

  rc = DSA_sign(1,hash,SHA_DIGEST_LENGTH,sigRet,&sigLen,dsa);

  if (debugLevel > 3)
    {
      char *hv;
      printf("siglen = %d\n",sigLen);
      hv = bin2hex(sigRet,sigLen);
      printf("sigret: %s\n",hv);
    }

  char *sigStr;
  sigStr = bin2hex(sigRet,sigLen);
  memcpy(&amCert.signature,sigStr,sigLen*2);
  sprintf(tmpStr,"%03d",sigLen*2);
  memcpy(&amCert.sigSize,tmpStr,3);
  fp = fopen(amCertFile,"w");
  if (fp)
    {
      fwrite(&amCert,sizeof(amCert),1,fp);
      fclose(fp);
    }
 
  return(0);

}
