#include <stdio.h>

#include "tqsl.h"
#include "sign.h"
extern int debugLevel;
int main(int argc,char *argv[])
{
  if (argc != 2)
    return(-1);

  TqslPublicKey  *pk;
  char  typ;
  pk = (TqslPublicKey *)readPubKey(argv[1],&typ);
  dumpPubKey(pk,stdout);
  return(0);

}