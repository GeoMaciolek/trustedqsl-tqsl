
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

int main(int argc, char *argv[])
{

  DSA    	*dsa;
  int   	dsaSize;
  int    	count;
  unsigned long h;
  char		*p,*q;
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
static BIO *bio_err = NULL;

bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);

  if (argc != 3)
    {
      printf("usage: chkcert amCert CA-Cert \n");
      return(-1);
    }
  strncpy(hamPKf,argv[1],99);
  strncpy(certKeyFn,argv[2],99);

  memset(&cert,' ',sizeof(cert));  
  rc= readCert(hamPKf,&cert);
  if (rc > 0)
    {
      if (cert.data.certType != '1')
	{
	  fprintf(stderr,"Invalid cert type %c\n",cert.data.certType);
	  return(1);
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
  //DSA_print(bio_err,dsa,0);
  //char typ;
  PublicKey *cpk;
  //printf("cert file %s\n",certKeyFn);
  cpk = (PublicKey *)readQpub(certKeyFn,&typ);
	


  BN_hex2bn(&dsa->pub_key,(const char *)cpk->pkey);

  //DSA_print(bio_err,dsa,0);
  unsigned char hash[40];
  SHA1((unsigned char *)&cert.data,sizeof(cert.data),hash);
  //printf("hash: %s\n",bin2hex(hash,SHA_DIGEST_LENGTH));
  unsigned char sigRet[500];
  unsigned int sigLen;
  char	sigsize[5];
  for(i=0;i<3;i++)
    {
      if (cert.sigSize[i] == ' ')
	{
	  sigsize[i] = '\0';
	  break;
	}
      sigsize[i] = cert.sigSize[i];
    }

  sigLen = atol(sigsize);
  hex2bin(cert.signature,sigRet,sigLen);
  
  sigLen = (sigLen/2);
  //printf("hash: %s\n",bin2hex(hash,SHA_DIGEST_LENGTH)); //memory leak
  //printf("sigret: %d %s\n",sigLen,bin2hex(sigRet,sigLen));

  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);

  switch (rc)
    {
    case 1:
      printf("Good Cert!  verify = %d  signlen %d\n",rc,sigLen);
      break;
    case 0:
      printf("Bad Cert!  verify = %d  signlen %d\n",rc,sigLen);
      break;
    default:
      printf("Error validating!  verify = %d  signlen %d\n",rc,sigLen);
    }


  return(0);

}
