

/*
    TrustedQSL Digital Signature Library
    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifdef __BCPLUSPLUS__
#include <io.h>
#else
#include <unistd.h>
#endif
#include <ctype.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#include "tqsl.h"
#include "sign.h"

#ifdef __BCPLUSPLUS__
__declspec(dllimport) DSA * _DSA_new(void);
#endif
//
// All functions return 0 for failed and no zero on success
//

void initPublicKey(TqslPublicKey *pk)
{
  memset(pk,' ',sizeof(TqslPublicKey));
}
void initCert(TqslCert *cert)
{
   memset(cert,' ',sizeof(TqslCert));
}
static char cvsID[] = "$Id$";
int debugLevel=0;

int tqslReadCert(const char *fname,TqslCert *cert)
{

  char		buf[MaxCertSize];
  int		rc;
  int		fd;
#ifndef __BCPLUSPLUS__
  cvsID = cvsID;  // avoid warnigns
#endif

  fd = open(fname,O_RDONLY);
  if (fd < 0)
    return(0);

  rc = read(fd,buf,MaxCertSize);
  close(fd);  // we are done with it.
  if (rc <= 0)
    {
      return(0);
    }
  
  switch (buf[0])
    {
    case '1':
    case '0':
      memcpy(cert,buf,sizeof(TqslCert));
      return(1);
    default:
      return(0);
    }
  return(0);
}
int tqslWriteCert(const char *fname,TqslCert *cert)
{

  int		rc;
  int		fd;

  fd = open(fname,O_WRONLY);
  if (fd < 0)
    return(0);

  rc = write(fd,cert,sizeof(TqslCert));
  close(fd);  // we are done with it.
  if (rc >= 0)
    return(rc);
  else
    return(0);
}

// tqslGenNewKeys
//
// return 0 on error.  Even if 1 file was successful written.
//

int tqslGenNewKeys(const char *callSign,const char *privFile,
	       const char *pubFile)
{
  DSA    *dsa;
  char	*p;
  int 	i;
  char	newCall[CALL_SIZE+1];
  int 	callLen;
  int   rc;

  //
  // make sure call sign isn't too long
  //

  if ((int)strlen(callSign) > CALL_SIZE)
    callLen =  CALL_SIZE;
  else
    callLen = strlen(callSign);

  for(i=0;i < callLen;i++)
    newCall[i] = toupper(callSign[i]);

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  rc = DSA_generate_key(dsa);
  if (rc == 0)
    return(0);
  FILE *fp;

  // save public key file
  fp = fopen(pubFile,"w");
  if (fp)
    {
      TqslPublicKey pk;
      initPublicKey(&pk);

      pk.pkType = '1';
      p = BN_bn2hex(dsa->pub_key);
      // using memcpy instead of strcpy to avoid coping nulls
      memcpy(&pk.callSign,newCall,strlen(callSign));
      memcpy(&pk.pkey,p,strlen(p));
      pk.pubkeyNum[0] = '1';

      // should be move to fileio

      fwrite(&pk,sizeof(pk),1,fp);
      fclose(fp);
    }
  else
    return(0);

  fp = fopen(privFile,"w");
  if (fp)
    {
      p = BN_bn2hex(dsa->priv_key);
      fwrite (p,strlen(p),1,fp);
      fclose(fp);
    }     
  else
    return(0);

  return(1);

}


int tqslCheckCert(TqslCert *cert,TqslCert *CACert,int chkCA)
{
  DSA    	*dsa;
  int 		i;
  int		rc;

  if (cert->data.certType == '0' || CACert == NULL)  // then selfsigned
    CACert = cert;

  // do we need to check the CA Cert?
  if (chkCA > 0)
    {
      rc = tqslCheckCert(CACert,NULL,0);
      if (rc != 1)  // ca cert not valid
	return(rc);
    }
  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  char pkhex[200];
  strncpy(pkhex,(const char *)CACert->data.publicKey.pkey,pubKeySize);
  pkhex[pubKeySize] = '\0';

  BN_hex2bn(&dsa->pub_key,pkhex);

  unsigned char hash[40];
  unsigned char *amPtr;
  amPtr = (unsigned char *)cert;
  SHA1(amPtr,sizeof(cert->data),hash);


  unsigned char 	sigRet[500];
  unsigned int 		sigLen;
  char			sigsize[5];

  for(i=0;i<3;i++)
    {
      if (cert->sigSize[i] == ' ')
	{
	  sigsize[i] = '\0';
	  break;
	}
      sigsize[i] = cert->sigSize[i];
      sigsize[i+1] = '\0';
    }

  sigLen = atol(sigsize);
  hex2bin(cert->signature,sigRet,sigLen);
  
  sigLen = (sigLen/2);
  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);
  DSA_free(dsa);
  ERR_remove_state(0);  // clean up error queue

  if (rc >= 0)
    return(rc);
  else
    return(0);
}
int tqslSignCert(TqslCert *cert,const char *caPrivKey,
                 const char *caId,TqslPublicKey *pubKey,
                 const char *certNum,const char *issueDate,
                 char *expireDate,int selfSign)
{

  DSA    	*dsa;
  int		rc;
  char		tmpStr[50];

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  rc = BN_hex2bn(&dsa->priv_key,caPrivKey);
  if (rc == 0)
    {
      DSA_free(dsa);
      return(0);
    }

  memset(cert,0,sizeof(TqslCert));
  memset(cert,' ',sizeof(TqslCert));
  
  if (selfSign == 1)
    cert->data.certType = '0';
  else
    cert->data.certType = '1';

  cert->data.publicKey = *pubKey;
  sprintf(tmpStr,"%-10.10s       ",issueDate);
  memcpy(cert->data.issueDate,tmpStr,10);

  sprintf(tmpStr,"%-10.10s       ",expireDate);
  memcpy(cert->data.expireDate,tmpStr,10);

  
  sprintf(tmpStr,"%-10.10s       ",caId);
  memcpy(&cert->data.caID,tmpStr,10);

  sprintf(tmpStr,"%s      ",certNum);
  memcpy(&cert->data.caCertNum,tmpStr,6);

  unsigned char hash[40];
  SHA1((unsigned char *)&cert->data,sizeof(TqslCertData),hash);

  unsigned char sigRet[500];
  unsigned int sigLen;

  if (debugLevel > 0)
    {
      char *p;
      p = bin2hex(hash,SHA_DIGEST_LENGTH);
      printf("hash: %s\n",p); 
      printf("amcert: %s\n", (char *)cert);
    }

  rc = DSA_sign(1,hash,SHA_DIGEST_LENGTH,sigRet,&sigLen,dsa);
  
  if (debugLevel > 3)
    {
      char *hv;
      printf("siglen = %d\n",sigLen);
      hv = bin2hex(sigRet,sigLen);
      printf("sigret: %s\n",hv);
    }

  char *sigStr;
  sigStr = bin2hex(sigRet,sigLen);
  memcpy(cert->signature,sigStr,sigLen*2);
  sprintf(tmpStr,"%03d",sigLen*2);
  memcpy(cert->sigSize,tmpStr,3);
  DSA_free(dsa);

  if (rc >= 0)
    return(rc);
  else
    return(0);

}

int tqslReadPub(const char *fname,TqslPublicKey *pubkey)
{

  int		rc;
  int		fd;

  fd = open(fname,O_RDONLY);
  if (fd < 0)
    return(0);

  rc = read(fd,pubkey,sizeof(TqslPublicKey));
  close(fd);  // we are done with it.
  if (rc <= 0)
    {
      return(0);
    }
  
  return(1);

}

int tqslWritePub(const char *fname,TqslPublicKey *pubkey)
{

  int		rc;
  int		fd;

  fd = open(fname,O_WRONLY);
  if (fd < 0)
    return(0);

  rc = write(fd,pubkey,sizeof(TqslPublicKey));
  close(fd);  // we are done with it.
  return(1);
}

int tqslSignData(const char *privKey,const unsigned char *data,
		 TqslCert *cert,TqslSignature *signature, int len)
{

  DSA    	*dsa;
  int		rc;

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  rc = BN_hex2bn(&dsa->priv_key,privKey);
  if (rc == 0)
    {
      DSA_free(dsa);
      return(0);
    } 


  memset(signature,0,sizeof(TqslSignature));
  memset(signature,' ',sizeof(TqslSignature));


  unsigned char hash[40];


  SHA1(data,len,hash);

  unsigned char 	sigRet[500];
  unsigned int 		sigLen;
  char 			*hexSign;

  rc = DSA_sign(1,hash,SHA_DIGEST_LENGTH,sigRet,&sigLen,dsa);
  DSA_free(dsa);
  if (rc <= 0)
    return(0);

  hexSign = bin2hex(sigRet,sigLen);
  memcpy(&signature->signature,hexSign,strlen(hexSign));
  signature->sigType = '1';
  char tStr[20];
  sprintf(tStr,"%d    ",sigLen*2);
  memcpy(signature->sigSize,tStr,3);
  signature->cert = *cert;

  return(1);

  
}
int tqslVerifyData(unsigned char *data,TqslSignature *signature,int len)
{
  DSA    	*dsa;
  int		rc;

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  char pkhex[200];
  strncpy(pkhex,(const char *)signature->cert.data.publicKey.pkey,pubKeySize);
  pkhex[pubKeySize] = '\0';
  BN_hex2bn(&dsa->pub_key,pkhex);

  unsigned char hash[40];

  SHA1(data,len,hash);

  unsigned char 	sigRet[500];
  unsigned int 		sigLen;
  char			sigsize[5];

  for(int i=0;i<3;i++)
    {
      if (signature->sigSize[i] == ' ')
	{
	  sigsize[i] = '\0';
	  break;
	}
      sigsize[i] = signature->sigSize[i];
      sigsize[i+1] = '\0';
    }

  sigLen = atol(sigsize);
  hex2bin(signature->signature,sigRet,sigLen);
  
  sigLen = (sigLen/2);

  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);
  DSA_free(dsa);
  if (rc <= 0)
    return(0);

  return(1);

}
