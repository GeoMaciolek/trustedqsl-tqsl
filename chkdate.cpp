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

#include "sign.h"

#include <stdio.h>
extern int debugLevel;
int main(int argc,char *argv[])
{

  if (argc != 2)
    return(1);
  fprintf(stderr,"chkDate %s\n",argv[1]);
  printf("chkDate returned %d\n",chkDate(argv[1]));
  return(0);
}
