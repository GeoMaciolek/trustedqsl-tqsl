
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
#include <openssl/dsa.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "sign.h"
#include "amcert.h"
// #include <openssl/engine.h>
extern int errno;
static char cvsID[] = "$Id$";
int debugLevel;
static BIO *bio_err = NULL;
int main(int argc, char *argv[])
{

  DSA    *dsa;
  char	*p;
  int 	i;
  char	callsign[200];

  printf("sizeof Amcert: %d\n",sizeof(TqslCert));

  if (argc != 2)
    {
      fprintf(stderr,"genkey callsign\n");
      return(2);
    }
  
  bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);
  strcpy(callsign,argv[1]);
  for(i=0;i < (int)strlen(callsign);i++)
    callsign[i] = toupper(callsign[i]);
  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(1);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);


  DSA_generate_key(dsa);
  FILE *fp;

  char fname[50];
  sprintf(fname,"%s.pub",argv[1]);
  fp = fopen(fname,"w");
  if (fp)
    {
      TqslPublicKey pk;
      initPublicKey(&pk);
      printf("writing public key file %s\n",fname);
      //      BN_print_fp(fp,dsa->p);
      DSA_print(bio_err,dsa,0);
      p = BN_bn2hex(dsa->pub_key);
      //      if (debugLevel > 0)
	printf("strlen of pubkey is %d\n",strlen(p));
      //      fprintf(fp,"1%12.12s%5d%s",callsign,1,p);
      pk.pkType = '1';
      memcpy(&pk.callSign,callsign,strlen(callsign));
      memcpy(&pk.pkey,p,strlen(p));
      pk.pubkeyNum[0] = '1';

      // should be move to fileio

      fwrite(&pk,sizeof(pk),1,fp);
      
      printf("\n%s\n",p);
	  
      fclose(fp);
    }

  sprintf(fname,"%s.prv",argv[1]);
  fp = fopen(fname,"w");
  if (fp)
    {
      printf("writing private key file %s\n",fname);
      p = BN_bn2hex(dsa->priv_key);
      fprintf(fp,"%s",p);
	  
      fclose(fp);
    }      
  sprintf(fname,"%s.hpub",argv[1]);
  fp = fopen(fname,"w");
  if (fp)
    {
      printf("writing private key file %s\n",fname);
      p = BN_bn2hex(dsa->pub_key);
      fprintf(fp,"%s",p);
	  
      fclose(fp);
    }    
  return(0);

}
