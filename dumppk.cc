#include <stdio.h>

#include "amcert.h"
#include "sign.h"
int debugLevel = 0;
int main(int argc,char *argv[])
{
  if (argc != 2)
    return(-1);

  PublicKey  *pk;
  char  typ;
  pk = (PublicKey *)readQpub(argv[1],&typ);
  dumpPubKey(pk,stdout);
  return(0);

}
