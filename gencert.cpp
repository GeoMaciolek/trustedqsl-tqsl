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

char *readPrivKey(const char *fname);
int chkDate(const char *sdate);

extern int errno;
extern int debugLevel;
static char cvsID[] = "$Id$";

// private key should be stored encrypted.  But for this example it
// is stored as an hex encoded string.


int main(int argc, char *argv[])
{

  char		*privKey;
  TqslPublicKey pubkey;
  TqslCert	amCert;

  int		rc;
  char		amPkFile[100];
  char		caPrivFile[100];
  char		amCertFile[100];
  char		certNum[10];
  int 		optCnt=0;
  int 		c,errFlg=0;
  int		selfSign=0;
  char		expStr[20];
  char		issueStr[20];
  char		caId[20];

  cvsID = cvsID; // prevent warnings

  while ((c = getopt(argc, argv, "sC:p:a:n:d:e:i:I:")) != EOF)
    switch (c) 
      {
      case 'i':
	strcpy(issueStr,optarg);
	if (!chkDate(issueStr))
	  {
	    fprintf(stderr,"invalid issued date %s\n",issueStr);
	    errFlg++;  
	  }
	else
	  optCnt++;

	break;
      case 'e':
	strcpy(expStr,optarg);
	if (!chkDate(expStr))
	  {
	    fprintf(stderr,"invalid expire date %s\n",expStr);
	    errFlg++;  
	  }
	else
	  optCnt++;
	break;
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
      case 'I':
	strncpy(caId,optarg,10);
	caId[10]='\0';
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

  if (errFlg || optCnt != 7)
    {
      printf("usage: gencert -s -a Amateur-cert -p amateur-PK -C "
	     "CA-priv-key -n cert# -i issue-date "
	     "-e expire-date -I CA ID (10 max)\n\n");
      printf("-s              - Self Signed Cert\n");
      printf("-a Amateur-cert - a file containing the amateur's cert\n");
      printf("-p Amateur-PK   - a file containing the amateur public key\n");
      printf("-C CA-priv-key  - a file containing the CA's private key\n");
      printf("-n number       - the cert number assigned by the CA\n");
      printf("-i issue-date   - the date which the cert was created (mm/dd/yyyy)\n");
      printf("-e expire-date  - the date which the cert will expire (mm/dd/yyyy)\n");
      printf("-I CA ID        - The CA's ID string must be the same for all certs.\n");
      printf("\n");

      return(-1);
    }

  rc = tqslReadPub(amPkFile,&pubkey);
  if (rc == 0)
    {
      fprintf(stderr,"Unable to read public key file %s\n",amPkFile);
      return(2);
    }

  privKey = readPrivKey(caPrivFile);
  if (privKey == NULL)
    return(-4);


  // warning debug call.  You must change the calls to real calls of
  // the CA rep that signs the cert.

  fprintf(stderr,"Warning!!! Sign with test call signs\n");

  rc = tqslSignCert(&amCert,privKey,caId,&pubkey,certNum,issueStr,
		    expStr,selfSign,"W1TST","K2TST","N3TST");

  
  if (rc < 1)
    {
      printf("Signed failed\n");
      return(1);
    }

  printf("Signed successful\n");

  rc = tqslWriteCert(amCertFile,&amCert);
  if (rc <= 0)
    {
      fprintf(stderr,"Couldn't write cert file for %s\n",amCertFile);
      return(-2);
    }
  return(0);

}
