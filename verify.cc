
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
#include <openssl/sha.h>
#include <openssl/dsa.h>
#include "sign.h"
#include "amcert.h"

// #include <openssl/engine.h>
extern int errno;
int debugLevel;
static char cvsID[] = "$Id$";


static BIO *bio_err = NULL;
#if 0
static void  dsa_cb(int p, int n, void *arg)
{
  char c='*';
  static int ok=0,num=0;

  if (p == 0) 
    { 
      c='.'; 
      num++; 
    }

  if (p == 1) 
    c='+';

  if (p == 2) 
    { 
      c='*'; 
      ok++; 
    }

  if (p == 3) 
    c='\n';

  BIO_write((BIO *)arg,&c,1);
  (void)BIO_flush((BIO *)arg);

  if (!ok && (p == 0))
    {
      //      BIO_printf((BIO *)arg,"error in dsatest ok = %d - p = %d - num = %d\n",
      //	 ok,p,num);
      //    fprintf(stderr,"in somthing that shouldn't exit\n");
      return;
    }
}
#endif
//static char  msg[] = "This is a test of signing";
int main(int argc, char *argv[])
{
  unsigned char	hash[40];
  DSA    *dsa;
  //  int    dsaSize;
  int    rc;
  //  unsigned long h;
  //  char	*p,*q;


  if (argc == 2)
    {
      rc = sha1File(argv[1],hash);
      //      if (rc != 0)
	//	printf("%s",hash);
    }


  bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);

  dsa = DSA_new();

  unsigned char sigRet[100];
  //  unsigned char sigTest[100];
  unsigned int	sigLen;

  if (dsa != NULL)
    {

      BN_hex2bn(&dsa->p,pVal);
      BN_hex2bn(&dsa->g,gVal);
      BN_hex2bn(&dsa->q,qVal);

      readBig("pubkey.txt",&dsa->pub_key);
      DSA_print(bio_err,dsa,0);
      //      sigLen = readSign(argv[1],sigRet,99);
      sigLen = readSignAsc(argv[1],sigRet,99);
	
      if (sigLen > 0)
	{
	  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);
	  printf("verify returned %d\n",rc);
	}
      else
	{
	  perror("readSignAsc");
	  printf("bad read of sig file\n");
	}
    }
  else
    {
      fprintf(stderr,"dsa = NULL\n");
      return(-1);
    }

  return(0);

}
