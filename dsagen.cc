/*
  dsagen program is used to create the inital static P, Q and G values.
  As such it should really never be used unless you are creating your
  own PKI outside of LOTW and TQSL.  

*/

#include <stdio.h>
#include <string.h>
#include <openssl/dsa.h>

void cb(int p, int n, void *v)
	{
	char c='*';

	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	printf("%c",c);
	fflush(stdout);
	}

int main()
{

  const char        copyright[] = 
"/* \n"
"    TrustedQSL Digital Signature Library\n"
"    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net and\n"
"                     ARRL\n"
"\n"
"    This library is free software; you can redistribute it and/or\n"
"    modify it under the terms of the GNU Lesser General Public\n"
"    License as published by the Free Software Foundation; either\n"
"    version 2.1 of the License, or (at your option) any later version.\n"
"\n"
"    This library is distributed in the hope that it will be useful,\n"
"    but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
"    Lesser General Public License for more details.\n"
"\n"
"    You should have received a copy of the GNU Lesser General Public\n"
"    License along with this library; if not, write to the Free Software\n"
"    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
"*/\n\n";

  DSA                *dsa;
  int                counter;
  unsigned long       h;
  BIO                *bio_err=NULL;

  if (bio_err == NULL)
    bio_err=BIO_new_fp(stderr,BIO_NOCLOSE);

  dsa=DSA_generate_parameters(1024,NULL,20,&counter,&h,cb,(void *)NULL);

  if (dsa != NULL)
    {
      FILE  *o;
      DSA_print(bio_err,dsa,0);
      
      o = fopen("dsa_static.h","a+");
      if (o)
	{
	  fprintf(o,"#ifndef DSA_STATIC_H\n#define DSA_STATIC_H\n\n");
	  //fprintf(o,copyright);
	  fprintf(o,"\n\n");
	  fprintf(o,"\nconst char pVal[] = \"%s\";\n",BN_bn2hex(dsa->p));
	  fprintf(o,"\nconst char qVal[] = \"%s\";\n",BN_bn2hex(dsa->q));
	  fprintf(o,"\nconst char gVal[] = \"%s\";\n",BN_bn2hex(dsa->g));


	  printf("bit length of p = %d\n",strlen(BN_bn2hex(dsa->p))*4);
	  printf("bit length of q = %d\n",strlen(BN_bn2hex(dsa->q))*4);
	  printf("bit length of g = %d\n",strlen(BN_bn2hex(dsa->g))*4);
	  fprintf(o,"\n\n#endif\n\n");
	  fclose(o);
	}
    }
      
}

