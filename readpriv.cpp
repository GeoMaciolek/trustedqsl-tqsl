
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
