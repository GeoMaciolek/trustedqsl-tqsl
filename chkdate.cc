
#include "sign.h"

#include <stdio.h>
int debugLevel=1;
int main(int argc,char *argv[])
{

  if (argc != 2)
    return(1);
  fprintf(stderr,"chkDate %s\n",argv[1]);
  printf("chkDate returned %d\n",chkDate(argv[1]));
  return(0);
}
