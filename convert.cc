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
