/* 
    TrustedQSL/LoTW client and PKI Library
    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net and
                     ARRL

    This library is free software; you can redistribute it and/or
    modify it under the terms of the Open License packaged with
    the source.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

    You should have received a copy of the Open License see 
    http://www.trustedQSL.org/ or contact wa1gon@arrl.net.

*/


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "tqsl.h"

extern int errno;

static char cvsID[] = "$Id$";

extern int	debugLevel;

int main(int argc, char *argv[])
{
  extern int 	optind;
  extern char 	*optarg;

  TqslCert	amCert;
  TqslCert	caCert;

  int		rc;
  char		amCertFile[100];
  char		caCertFile[100];
  int 		optCnt=0;
  int 		c,errFlg=0;

  cvsID = cvsID;

  while ((c = getopt(argc, argv, "C:a:d:s")) != EOF)
    switch (c) 
      {
      case 's':
	printf("size of cert is: %d\n",sizeof(amCert));
	printf("size of qsl signature is: %d\n",sizeof(TqslSignedQSL));
	return(1);
      case 'a':
	strcpy(amCertFile,optarg);
	optCnt++;
	break;
      case 'C':
	strcpy(caCertFile,optarg);
	optCnt++;
	break;
      case 'd':
	debugLevel = atol(optarg);
	break;
      default:
	errFlg++;
	break;
      }
  if (errFlg || (optCnt != 2))
    {
      printf("usage: chkcert -a amCert -C CA-Cert \n");
      return(-1);
    }

  rc= tqslReadCert(caCertFile,&caCert);
  if (rc > 0)
    {
      if (caCert.data.certType != '0')
	{
	  fprintf(stderr,"Invalid cert type %c for \n",caCert.data.certType);
	  return(1);
	}
    }

  rc= tqslReadCert(amCertFile,&amCert);
  if (rc > 0)
    {
      if ((amCert.data.certType != '1') && (amCert.data.certType != '0'))
	{
	  fprintf(stderr,"Invalid cert type %c for amaterur\n",
		  amCert.data.certType);
	  return(1);
	}
     if (amCert.data.certType == '0')
       printf("Self signed Cert.\n");
    } 

  rc = tqslCheckCert(&caCert, &amCert,1);
  switch (rc)
    {
    case 1:
      printf("Good Cert!  verify = %d\n",rc);
      break;
    case 0:
      printf("Bad Cert!  verify = %d\n",rc);
      break;
    default:
      printf("Error validating!  verify = %d\n",rc);
    }
  return(0);

}
