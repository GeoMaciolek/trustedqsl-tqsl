
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

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/dsa.h>
#include "amcert.h"
#include "sign.h"

// #include <openssl/engine.h>
extern int errno;
extern int debugLevel;
static char cvsID[] = "$Id$";
static BIO *bio_err = NULL;


void initPublicKey(PublicKey *pk)
{
   memset(pk,' ',sizeof(PublicKey));
}
void initCert(AmCertExtern *cert)
{
   memset(cert,' ',sizeof(AmCertExtern));
}

int validateCert(AmCertExtern *caCert, AmCertExtern *amCert)
{

  DSA    	*dsa;
  int 		i;
  int		rc;

  if ((debugLevel > 0))
    bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);

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

  char pkhex[200];
  strncpy(pkhex,(const char *)caCert->data.publicKey.pkey,pubKeySize);
  pkhex[pubKeySize] = '\0';

  BN_hex2bn(&dsa->pub_key,pkhex);
  if (debugLevel > 0)
    {
      DSA_print(bio_err,dsa,0);
    }

  unsigned char hash[40];
  unsigned char *amPtr;
  amPtr = (unsigned char *)amCert;
  SHA1(amPtr,sizeof(amCert->data),hash);

  if (debugLevel > 0)
    {
      char *p = bin2hex(hash,SHA_DIGEST_LENGTH);
      fprintf(stderr,"validateCert: hash: %s\n",p);
      free(p);
    }

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

  if (debugLevel > 0)
    {
      char *p = bin2hex(sigRet,sigLen);
      fprintf(stderr,"validateCert: \nsigLen: %d\n",sigLen);
      fprintf(stderr,"validateCert: \nsigRet: %s\n",p);
      free(p);
    }
  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);
  DSA_free(dsa);
  return(rc);

}
