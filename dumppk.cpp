#include <stdio.h>

#include "tqsl.h"
#include "sign.h"

extern int debugLevel;
int main(int argc,char *argv[])
{
  if (argc != 2)
    return(-1);

  TqslPublicKey  pk;

  int rc;
  rc = tqslReadPub(argv[1],&pk);

  printf("type: %c\n;",pk.pkType);
  printf("call sign: %s;\n",pk.callSign);
  printf("pubkey #: %s;\n",pk.pubkeyNum);
  printf("pubkey: \n%s;\n",pk.pkey);

  return(0);

}
