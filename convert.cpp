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
#include "tqsl.h"
#include "sign.h"

static char cvsID[] = "$Id$";

// a very simple date checking routine.  
// returns 1 if date is ok or 0 if date is bad.
//
int chkDate(const char *sdate)
{
  char dateA[3][10];

  size_t i;
  int j,k;
  i = 0;
  j = 0;
  k = 0;
  long d,y,m;

#ifndef __BCPLUSPLUS__
  cvsID = cvsID;
#endif
  for (i=0;i<strlen(sdate);i++)
    {
      if (sdate[i] == '\0')
	{
	dateA[j][k] = '\0';
	break;
	}
      if (sdate[i] == '/')
	{
	  dateA[j][k] = '\0';
	  k=0;
	  j++;
	  continue;
	}
      dateA[j][k] = sdate[i];
      k++;
      dateA[j][k] = '\0';
    }
  
  if (j != 2)
    return(0);

  m = atol(dateA[0]);
  d = atol(dateA[1]);
  y = atol(dateA[2]);

  if (d < 1 || d > 31)
    return(0);

  if (m < 1 || m > 12)
    return(0);

  if (y < 2001 || y > 2200)
    return(0);

  return(1);
  
}
char *bin2hex(const unsigned char *binStr,int len)
{
  char *tStr;
  char	value[4];
  tStr = (char *) malloc(len*3);

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

void dumpCert(TqslCert *cert,FILE *fp)
{
  fprintf(fp,"type: %c\n",cert->data.certType);
  fprintf(fp,"issue Date: %10.10s\n",cert->data.issueDate);
  fprintf(fp,"expire Date: %10.10s\n",cert->data.expireDate);
  fprintf(fp,"CA id: %10.10s\n",cert->data.caID);
  fprintf(fp,"CA Cert#: %6.6s\n",cert->data.caCertNum);
  // dumpPubKey(&cert->data.publicKey,fp);
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


