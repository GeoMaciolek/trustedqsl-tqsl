
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
#include <openssl/err.h>

#include "amcert.h"
#include "sign.h"

// #include <openssl/engine.h>
extern int errno;
static char cvsID[] = "$Id$";

#if 0
void initPublicKey(PublicKey *pk)
{
   memset(pk,' ',sizeof(PublicKey));
}
void initCert(AmCertExtern *cert)
{
   memset(cert,' ',sizeof(AmCertExtern));
}
#endif

int validiteCert(AmCertExtern *caCert, AmCertExtern *amCert)
{

  DSA    	*dsa;
  //  unsigned long h;
  // char		*p,*q;
  int 		i;
  char		callsign[200];
  char  	*pk;
  char		typ;
  PublicKey   	*pubkey;
  AmCertExtern	cert;
  //  char	     	*fname;
  char		*CAid;
  PublicKey	*caPK;
  int		rc;
  FILE		*fp;
  char		tmpStr[50];

  char		hamPKf[100];
  char		certNoStr[10];
  char		certKeyFn[100];


  if (amCert->data.certType == '0')  // then selfsigned
    caCert = amCert;
  else if (caCert == NULL)
    caCert = amCert;

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(1);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  //PublicKey *cpk;

  //cpk = (PublicKey *)readQpub(certKeyFn,&typ);

  BN_hex2bn(&dsa->pub_key,(const char *)caCert->data.publicKey.pkey);

  unsigned char hash[40];
  unsigned char *amPtr;
  amPtr = (unsigned char *)amCert;
  SHA1(amPtr,sizeof(amCert->data),hash);

  unsigned char sigRet[500];
  unsigned int sigLen;
  char	sigsize[5];
  for(i=0;i<3;i++)
    {
      if (amCert->sigSize[i] == ' ')
	{
	  sigsize[i] = '\0';
	  break;
	}
      sigsize[i] = amCert->sigSize[i];
    }

  sigLen = atol(sigsize);
  hex2bin(amCert->signature,sigRet,sigLen);
  
  sigLen = (sigLen/2);
  //printf("hash: %s\n",bin2hex(hash,SHA_DIGEST_LENGTH)); //memory leak
  //printf("sigret: %d %s\n",sigLen,bin2hex(sigRet,sigLen));

  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);
  DSA_free(dsa);
  return(rc);

}
