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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "amcert.h"
#include "sign.h"

static char cvsID[] = "$Id$";

char *bin2hex(const unsigned char *binStr,int len)
{
  char *tStr;
  char	value[4];
  tStr = (char *) malloc(len*2);

  if (tStr == NULL)
    return(tStr);

  int i,j=0;

  for(i=0;i<len;i++)
    {
      sprintf(value,"%02x",binStr[i]);
      strncpy(&tStr[j],value,2);
      j=j+2;
      tStr[j] = 0;
    }
  return(tStr);
}
void dumpPubKey(PublicKey *pk,FILE *fp)
{
  fprintf(fp,"type: %c\n",pk->pkType);
  fprintf(fp,"call sign: %10.10s\n",pk->callSign);
  fprintf(fp,"pubkey #: %6.6s\n",pk->pubkeyNum);
  fprintf(fp,"pubkey: \n%176.176s\n",pk->pkey);
  return;
}
void dumpCert(AmCertExtern *cert,FILE *fp)
{
  fprintf(fp,"type: %c\n",cert->data.certType);
  fprintf(fp,"issue Date: %10.10s\n",cert->data.issueDate);
  fprintf(fp,"expire Date: %10.10s\n",cert->data.expireDate);
  fprintf(fp,"CA id: %10.10s\n",cert->data.caID);
  fprintf(fp,"CA Cert#: %6.6s\n",cert->data.caCertNum);
  dumpPubKey(&cert->data.publicKey,fp);
  fprintf(fp,"sig size: %3.3s\n",cert->sigSize);
  fprintf(fp,"sig: \n%120.120s\n",cert->signature);
  return;
}
void hex2bin(char *hexStr,unsigned char *binStr,int len)
{
  long	v;
  int i;
  int j=0;
  unsigned char	b[3];

  b[2] = 0;
  for(i=0;i<len;i=i+2)
    {
      b[0] = hexStr[i];
      b[1] = hexStr[i+1];
      v = (unsigned char )strtol((const char *)b,NULL,16);
      binStr[j] = (unsigned char )v;
      j++;
      binStr[j]=0;
    }
}
