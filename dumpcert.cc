#include <stdio.h>

#include "amcert.h"
#include "sign.h"
int debugLevel = 0;
int main(int argc,char *argv[])
{
  if (argc != 2)
    return(-1);

  AmCertExtern 	cert;
  readCert(argv[1],&cert);
  dumpCert(&cert,stdout);
  return(0);

}
