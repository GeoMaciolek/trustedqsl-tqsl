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

// private key should be stored encrypted.  But for this example it
// is stored as an hex encoded string.

#include <stdio.h>
#include <string.h>
char *readPrivKey(const char *fname)
{
  FILE *fkey;
  char p[500];
  char *retStr;
  int rc;

  fkey = fopen(fname,"r");
  rc = -1;
  if (fkey)
    {
      retStr = fgets(p,499,fkey);
      if (retStr != NULL)
        {
          //strip out eol
          retStr = strchr(p,'\n');
          if (retStr != NULL)
            *retStr = '\0';
          retStr = strchr(p,'\r');
          if (retStr != NULL)
            *retStr = '\0';  

	  retStr = strdup(p);
	  return(retStr);
        }

      fclose(fkey);
      return(NULL);
    }

  return(NULL);

}
