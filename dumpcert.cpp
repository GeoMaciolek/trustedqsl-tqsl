#include <stdio.h>

#include "tqsl.h"
extern int debugLevel;
int main(int argc,char *argv[])
{
  if (argc != 2)
    return(-1);

  TqslCert 	cert;

  tqslReadCert(argv[1],&cert);

  printf("Certificate Information\n\n");
  printf("Type:                %c\n",cert.data.certType);
  printf("Issue Date:          %s\n",cert.data.issueDate);
  printf("Expire Date:         %s\n",cert.data.expireDate);
  printf("CA id:               %s\n",cert.data.caID);
  printf("CA Cert#:            %s\n",cert.data.caCertNum);


  printf("Amateur signature 1: %s\n",cert.data.caCall1);
  printf("Amateur signature 2: %s\n",cert.data.caCall2);
  printf("Amateur signature 3: %s\n",cert.data.caCall3);
  printf("\nSignature: \n%s\n",cert.signature);


  printf("Public Key Information:\n\n");
  printf("Type: %c;\n",cert.data.publicKey.pkType);
  printf("Call sign: %s;\n",cert.data.publicKey.callSign);
  printf("Pubkey #: %s;\n",cert.data.publicKey.pubkeyNum);
  printf("Pubkey: \n%s;\n\n",cert.data.publicKey.pkey);

  return(0);

}
