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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tqsl.h"

extern int errno;

static char cvsID[] = "$Id$";
extern int debugLevel;

int main(int argc, char *argv[])
{

  char		callsign[20];
  int   	c,errFlg=0,optCnt=0;
  char		pubFile[100];
  char		secretFile[100];
  char		pkNum[20];
  int		rc;

  cvsID = cvsID;

  memset(pubFile,0,sizeof(pubFile));
  memset(secretFile,0,sizeof(secretFile)); 
  memset(callsign,0,sizeof(callsign));

  while ((c = getopt(argc, argv, "c:s:p:n:")) != EOF)
    switch (c) 
      {
      case 's':
	strncpy(secretFile,optarg,99);
	optCnt++;
	break;
      case 'c':
        strncpy(callsign,optarg,19);
        optCnt++;
        break;
      case 'p':
        strncpy(pubFile,optarg,99);
	optCnt++;
	break;
      case 'n':
	strcpy(pkNum,optarg);
	optCnt++;
	break;
      default:
	errFlg++;
      }

  if (optCnt != 4 || errFlg != 0)
    {
      printf("Usage: %s -s private-file -p public-file -c call-sign -n pk#\n",
	     argv[0]);
      return(1);
    }

  TqslPublicKey pubKey;
  char		*secretKey;
    
  rc = tqslGenNewKeys(callsign,&secretKey,&pubKey);
  if (rc <= 0)
    {
      fprintf(stderr,"Creation of public/private key pair failed! %d\n",rc);
      return(rc);
    }

  strcpy(pubKey.pubkeyNum,pkNum);

  int fd;
  FILE *fout;

  rc = tqslWritePub(pubFile,&pubKey);
  if (rc < 1)
    fprintf(stderr,"Failed to write public key file %s\n",pubFile);
   


  fd = open(secretFile,O_WRONLY|O_CREAT|O_TRUNC,0600);
  if (fd >= 0)
    {
      rc = write(fd,secretKey,strlen(secretKey));
      close(fd);
      if (rc != (int)strlen(secretKey))
	{
	  fprintf(stderr,"writing private file failed\n");
	  return(1);
	}
    }
  else
    {
      fprintf(stderr,"creating private file failed\n");
      return(2);
    }	 
  return(0);

}
