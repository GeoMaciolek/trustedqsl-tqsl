
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

// #include <openssl/engine.h>
extern int errno;
static char cvsID[] = "$Id$";


int writeBig(const char *fname,const BIGNUM *bn)
{
  FILE *fkey;
  char *p;

  fkey = fopen(fname,"w");
  if (fkey)
    {
      p = BN_bn2hex(bn);
      fprintf(fkey,"%s\n",p);
      fclose(fkey);
      return(0);
    }
  return(errno);
}
static BIO *bio_err = NULL;
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

static char  msg[] = "This is a test of signing";
int main(int argc, char *argv[])
{

  DSA    *dsa;
  int    dsaSize;
  int    count;
  unsigned long h;
  char	*p,*q;

  bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);

  dsa = DSA_generate_parameters(704,NULL,0,&count,&h,dsa_cb,bio_err);
  // dsaSize = DSA_size(
  if (dsa != NULL)
    {
      DSA_generate_key(dsa);
      DSA_print(bio_err,dsa,0);
      FILE *fp;
      fp = fopen("dsa.dat","w");
      if (fp)
	{
	  //BN_print_fp(fp,dsa->p);
	  p = BN_bn2hex(dsa->p);
	  printf("[%s]\n",p);
	  
	  fclose(fp);
	}
      
    }
  else
    {
      fprintf(stderr,"dsa = NULL\n");
      return(-1);
    }

  writeBig("pfile.txt",dsa->p);
  writeBig("qfile.txt",dsa->q);
  writeBig("gfile.txt",dsa->g);
  writeBig("pubkey.txt",dsa->pub_key);
  writeBig("privkey.txt",dsa->priv_key);
 
  return(0);

}
