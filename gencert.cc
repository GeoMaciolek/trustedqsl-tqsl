
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

  if (argc != 4)
    {
      printf("usage: gencert amPK CA-Priv cert-#\n");
      return(-1);
    }
  strncpy(hamPKf,argv[1],99);
  strncpy(certKeyFn,argv[2],99);
  strncpy(certNoStr,argv[3],9);

  /* #if 0
     static struct option long_options[] =
     {
     { "amateur-public-key", required_argument, NULL, 'p' },
     { "cert-private-key", required_argument, NULL, 'C' },
     { "cert-number",required_argument, NULL, 'n' },
     { 0, 0, 0, 0 }
     };

     while((c = getopt_long(argc,argv,"p:C:n:")) != EOF)
     {
     switch(c)
     {
     case 'p':
     strncpy(hamPKf,optarg,99);
     break;
     case 'C':
     strncpy(certKeyFn,optarg,99);
     break;
     case 'n':
     strncpy(certNoStr,optarg,9);
     break;
     case '?':
     printf ("Try `%s -p amateurs-public-key file "
     "-C CA Priv Key file -n cert-num\n"), argv[0]);
     return (0);
     break;
     }

     }
     #endif
  */  

  pk = (char *)readQpub(hamPKf,&typ);
  if (pk != NULL)
    {
      if (typ = '1')
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

  readBig(certKeyFn,&dsa->priv_key);

  memset(&cert,' ',sizeof(cert));

  cert.data.certType = '1';
  cert.data.publicKey = *pubkey;
  memcpy(&cert.data.issueDate,"03/03/2001",10);
  memcpy(&cert.data.expireDate,"03/03/2011",10);

  
  sprintf(tmpStr,"%-10.10s       ","TQSL");
  memcpy(&cert.data.caID,tmpStr,10);

  sprintf(tmpStr,"%s      ",certNoStr);
  memcpy(&cert.data.caCertNum,tmpStr,6);


  unsigned char hash[40];
  SHA1((unsigned char *)&cert.data,sizeof(cert.data),hash);

  unsigned char sigRet[500];
  unsigned int sigLen;

  printf("hash: %s\n",bin2hex(hash,SHA_DIGEST_LENGTH)); //memory leak

  rc = DSA_sign(1,hash,SHA_DIGEST_LENGTH,sigRet,&sigLen,dsa);
  printf("siglen = %d\n",sigLen);
  printf("sigret: %s\n",bin2hex(sigRet,sigLen));
  char *sigStr;
  sigStr = bin2hex(sigRet,sigLen);
  memcpy(&cert.signature,sigStr,sigLen*2);
  sprintf(tmpStr,"%03d",sigLen*2);
  memcpy(&cert.sigSize,tmpStr,3);
  fp = fopen("cert.txt","w");
  if (fp)
    {


      // printf("writing public key file %s\n",fname);
      //BN_print_fp(fp,dsa->p);
      // p = BN_bn2hex(dsa->pub_key);
      fwrite(&cert,sizeof(cert),1,fp);
	  
      fclose(fp);
    }
    
 
  return(0);

}
