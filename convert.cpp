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


static char base64_table[] =
	{ 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0'
	};

static char base64_pad = '=';

unsigned char *bin2base64(const unsigned char *str, int length, int *ret_length) 
{
  const unsigned char *current = str;
  int i = 0;
  unsigned char *result = (unsigned char *)malloc(((length + 3 - length % 3) * 4 / 3 + 1) * sizeof(char));

  while (length > 2) 
    { /* keep going until we have less than 24 bits */
      result[i++] = base64_table[current[0] >> 2];
      result[i++] = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
      result[i++] = base64_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
      result[i++] = base64_table[current[2] & 0x3f];

      current += 3;
      length -= 3; /* we just handle 3 octets of data */
    }

  /* now deal with the tail end of things */
  if (length != 0) 
    {
      result[i++] = base64_table[current[0] >> 2];
      if (length > 1) 
	{
	  result[i++] = base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
	  result[i++] = base64_table[(current[1] & 0x0f) << 2];
	  result[i++] = base64_pad;
	}
      else 
	{
	  result[i++] = base64_table[(current[0] & 0x03) << 4];
	  result[i++] = base64_pad;
	  result[i++] = base64_pad;
	}
    }
  if(ret_length) 
    {
      *ret_length = i;
    }
  result[i] = '\0';
  return result;
}

/* as above, but backwards. :) */
unsigned char *str2base64(const unsigned char *str, int length, int *ret_length) 
{
  const unsigned char *current = str;
  int ch, i = 0, j = 0, k;
  /* this sucks for threaded environments */
  static short reverse_table[256];
  static int table_built;
  unsigned char *result;

  if (++table_built == 1) 
    {
      char *chp;
      for(ch = 0; ch < 256; ch++) 
	{
	  chp = strchr(base64_table, ch);
	  if(chp) 
	    {
	      reverse_table[ch] = chp - base64_table;
	    } else 
	      {
		reverse_table[ch] = -1;
	      }
	}
    }

  result = (unsigned char *)malloc(length + 1);
  if (result == NULL) 
    {
      return NULL;
    }

  /* run through the whole string, converting as we go */
  while ((ch = *current++) != '\0') 
    {
      if (ch == base64_pad) break;

      /* When Base64 gets POSTed, all pluses are interpreted as spaces.
	 This line changes them back.  It's not exactly the Base64 spec,
	 but it is completely compatible with it (the spec says that
	 spaces are invalid).  This will also save many people considerable
	 headache.  - Turadg Aleahmad <turadg@wise.berkeley.edu>
      */

      if (ch == ' ') ch = '+'; 

      ch = reverse_table[ch];
      if (ch < 0) continue;

      switch(i % 4) 
	{
	case 0:
	  result[j] = ch << 2;
	  break;
	case 1:
	  result[j++] |= ch >> 4;
	  result[j] = (ch & 0x0f) << 4;
	  break;
	case 2:
	  result[j++] |= ch >>2;
	  result[j] = (ch & 0x03) << 6;
	  break;
	case 3:
	  result[j++] |= ch;
	  break;
	}
      i++;
    }

  k = j;
  /* mop things up if we ended on a boundary */
  if (ch == base64_pad) 
    {
      switch(i % 4) 
	{
	case 0:
	case 1:
	  free(result);
	  return NULL;
	case 2:
	  k++;
	case 3:
	  result[k++] = 0;
	}
    }
  if(ret_length) 
    {
      *ret_length = j;
    }
  result[k] = '\0';
  return result;
}
