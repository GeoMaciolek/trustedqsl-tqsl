#include <stdio.h>

#include "tqsl.h"
extern int debugLevel;
int main(int argc,char *argv[])
{
  if (argc != 2)
    return(-1);

  TqslCert 	cert;

  tqslReadCert(argv[1],&cert);

  printf("type: %c\n",cert.data.certType);
  printf("issue Date: %s\n",cert.data.issueDate);
  printf("expire Date: %s\n",cert.data.expireDate);
  printf("CA id: %s\n",cert.data.caID);
  printf("CA Cert#: %s\n",cert.data.caCertNum);
  // dumpPubKey(&cert->data.publicKey,fp);
  // printf("sig size: %s\n",cert.sigSize);
  printf("sig: \n%s\n",cert.signature);


  printf("type: %c;\n",cert.data.publicKey.pkType);
  printf("call sign: %s;\n",cert.data.publicKey.callSign);
  printf("pubkey #: %s;\n",cert.data.publicKey.pubkeyNum);
  printf("pubkey: \n%s;\n",cert.data.publicKey.pkey);

  return(0);

}
