
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


#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>

#include "sign.h"
#include "tqsl.h"
// #include <openssl/engine.h>
extern int errno;
static char cvsID[] = "$Id$";
extern int debugLevel;

int main(int argc, char *argv[])
{

  char		callsign[20];
  int   	c,errFlg=0,optCnt=0;
  char		pubFile[100];
  char		secretFile[100];
  int		rc;

  cvsID = cvsID;

  memset(pubFile,0,sizeof(pubFile));
  memset(secretFile,0,sizeof(secretFile)); 
  memset(callsign,0,sizeof(callsign));

  while ((c = getopt(argc, argv, "c:s:p:")) != EOF)
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
      default:
	errFlg++;
      }

  if (optCnt != 3 || errFlg != 0)
    {
      printf("Usage: %s -s private-file -p public-file -c call-sign\n",
	     argv[0]);
      return(1);
    }

  rc = tqslGenNewKeys(callsign,secretFile,pubFile);
  if (rc <= 0)
    {
      fprintf(stderr,"Creation of public/private key pair failed! %d\n",rc);
      return(rc);
    }

  return(0);

}
